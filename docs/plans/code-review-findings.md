# krakenapi — Code Review Findings

**Date:** 2026-03-28
**Scope:** Full codebase review — safety, thread-safety, error handling, performance, test coverage

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 3     |
| High     | 5     |
| Medium   | 8     |
| Low      | 4     |

---

## Critical Issues

### C1. Exception in pending handler leaves promise permanently unfulfilled

**Files:** `kraken_ws_client.inl:89-91`, `kraken_ws_client.inl:149-170`

If `Resp::from_json(j)` throws inside a pending handler lambda, the promise is never
set — neither `set_value()` nor `set_exception()` is called. Any thread blocked on
`fut.get()` or `fut.wait_for()` will hang forever (or until the timeout in `execute()`).
The `subscribe_async` path is worse: `SubscribeResponse::from_json(j)` can also throw,
and the promise there carries a `pair<WsResponse, SubscriptionHandle>`.

**Impact:** Indefinite blocking or thread starvation from malformed server responses.

**Fix:** Wrap the body of every pending handler in `try/catch` and call
`prom->set_exception(std::current_exception())` on failure.

---

### C2. Data race on `disconnect_cb_`

**Files:** `kraken_ws_client.cpp:47`, `kraken_ws_client.cpp:56-57`, `kraken_ws_client.hpp:265`

`disconnect_cb_` is a `std::function<void(std::string)>` accessed from two threads
without synchronization:

- **Write:** `set_on_disconnect()` (line 57) — called from user thread.
- **Read + invoke:** on_close lambda (line 47) — called from ixwebsocket thread.

`std::function` is not thread-safe for concurrent read/write. A torn write can crash.
The comment on line 254 says "must be set before the connection opens", but this is not
enforced by the API — nothing prevents a user from calling `set_on_disconnect()` after
`connect()`.

**Impact:** Undefined behaviour / crash if `set_on_disconnect()` is called while a
disconnect event is being processed.

**Fix:** Protect `disconnect_cb_` with a mutex, or make the API enforce the documented
ordering (e.g. accept the callback in the constructor or `init()`).

---

### C3. Pending handler leak on execute timeout

**Files:** `kraken_ws_client.inl:100-108`

When `execute()` times out, the handler lambda remains in the `pending_` map:

```cpp
if (fut.wait_for(timeout) == std::future_status::timeout) {
    WsResponse<…> err;
    err.ok    = false;
    err.error = "request timed out";
    return err;       // ← pending_[id] still holds a shared_ptr<promise>
}
```

The abandoned handler holds a `shared_ptr<promise>` whose future is already dropped.
If the server eventually responds, `prom->set_value()` is called on a promise whose
future is gone — this is safe per the standard, but the handler and its captured state
live in `pending_` indefinitely, leaking memory for every timed-out request.

**Impact:** Unbounded memory growth under sustained request timeouts.

**Fix:** After timeout, erase the entry from `pending_` (requires the `id` to be
accessible; store it before calling `execute_async`). The same issue applies to
`subscribe()` timeouts.

---

## High Issues

### H1. `enqueue_or_send` / `on_open_handler` race window

**Files:** `kraken_ws_client.cpp:69-76`, `kraken_ws_client.cpp:78-87`

The queue mutex correctly serialises `enqueue_or_send` against `on_open_handler`.
However, `on_open_handler` sends queued messages **outside** the lock:

```cpp
void KrakenWsClient::on_open_handler() {
    std::vector<std::string> queued;
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        connected_.store(true);
        queued = std::move(send_queue_);
    }
    for (const auto& msg : queued)
        conn_->send(msg);       // ← outside lock; concurrent with enqueue_or_send's send
}
```

After the lock is released, `enqueue_or_send` sees `connected_==true` and calls
`conn_->send()` concurrently with the loop above. If `IWsConnection::send()` is not
thread-safe, this is a data race. `ix::WebSocket::send()` happens to be thread-safe,
but `IWsConnection::send()` makes no such guarantee — a different implementation
(or a mock) could break.

**Impact:** Data race if `send()` is not thread-safe; message reordering otherwise.

**Fix:** Either document thread-safety as a requirement of `IWsConnection::send()`,
or hold the lock during the flush loop (acceptable since sends are fast).

---

### H2. No curl timeout configuration

**File:** `kraken_rest_client.cpp:55-91`

`curl_perform` sets no `CURLOPT_TIMEOUT`, `CURLOPT_CONNECTTIMEOUT`, or
`CURLOPT_LOW_SPEED_LIMIT`. A stalled or unresponsive Kraken API server will block
the calling thread indefinitely.

**Impact:** Thread hangs; in single-threaded callers the entire program freezes.

**Fix:** Set `CURLOPT_CONNECTTIMEOUT` (e.g. 10 s) and `CURLOPT_TIMEOUT` (e.g. 30 s),
ideally configurable via the `KrakenRestClient` constructor.

---

### H3. `curl_slist_append` failure leaks the list

**File:** `kraken_rest_client.cpp:64-67`

```cpp
chunk.reset(curl_slist_append(chunk.release(), …));
```

If `curl_slist_append` returns `NULL` (allocation failure), `chunk.release()` has
already surrendered ownership — the original list is leaked.

**Impact:** Memory leak on allocation failure.

**Fix:** Capture the released pointer in a local, check the return value, and free it
manually on failure:

```cpp
auto* old = chunk.release();
auto* next = curl_slist_append(old, header.c_str());
if (!next) { curl_slist_free_all(old); throw std::bad_alloc(); }
chunk.reset(next);
```

---

### H4. 55 unguarded `std::stod` calls across response parsers

**Files:** `kraken_rest_api.hpp` (34 occurrences), `kraken_types.hpp` (21 occurrences)

The Kraken REST API returns monetary values as JSON strings (e.g. `"1.5"`). All
`from_json()` methods use `std::stod()` without exception handling:

```cpp
o.vol      = std::stod(j.value("vol", "0"));
o.vol_exec = std::stod(j.value("vol_exec", "0"));
```

`std::stod` throws `std::invalid_argument` on non-numeric input and
`std::out_of_range` on overflow. A single malformed field in any API response crashes
the caller with an unhandled exception.

**Impact:** Any unexpected string value (e.g. `"N/A"`, `""`, `"inf"`) in a Kraken
response causes a crash instead of a graceful `RestResponse::ok == false`.

**Fix:** Wrap conversions in a helper that catches and returns a default or signals
an error via `RestResponse::errors`. Alternatively, use `std::from_chars` which
reports errors via return code.

---

### H5. `KrakenRestClient` is not thread-safe

**File:** `kraken_rest_client.cpp:55-91`, `kraken_rest_client.hpp`

`KrakenRestClient` stores a single `CURL*` handle and reuses it for every `execute()`
call. Calling `execute()` concurrently from multiple threads produces a data race on
the curl handle (libcurl documents that a single handle must not be shared across
threads).

The class has no mutex, no documentation about thread-safety, and no per-request handle
creation.

**Impact:** Undefined behaviour if two threads call `execute()` at the same time.

**Fix:** Either document as single-threaded, add a mutex around `curl_perform`, or
create a thread-local / per-request curl handle.

---

## Medium Issues

### M1. Inconsistent JSON parsing patterns

**Files:** `kraken_types.hpp`, `kraken_rest_api.hpp`, `kraken_ws_api.hpp`

Three different patterns are used to read JSON fields:

1. `j.at("key")` — throws `json::out_of_range` if missing.
2. `j.value("key", default)` — returns default if missing.
3. `j.contains("key") && j["key"]` — guarded access.

These are mixed freely within the same `from_json()` method (e.g.
`Triggers::from_json` uses `.at()` while `OrderInfo::from_json` uses `.value()`).
Required fields should consistently use `.at()` and optional fields should consistently
use `.value()` — currently it's unpredictable which missing field throws vs. silently
defaults.

---

### M2. Array index access without bounds check

**File:** `kraken_rest_api.hpp` — `TickerInfo::from_json`

```cpp
if (j.contains("a")) t.ask = std::stod(j["a"][0].get<std::string>());
```

Checks that `"a"` exists but not that it's a non-empty array. If the API returns
`"a": []`, the `[0]` access is undefined behaviour (nlohmann/json asserts in debug,
throws in release).

Same pattern for `"b"`, `"c"`, `"v"`, `"p"`, `"t"`, `"l"`, `"h"`, `"o"`.

---

### M3. Credentials file not checked for permissions

**File:** `kraken_rest_api.hpp` — `Credentials::from_file`

The file at `~/.kraken/<name>` is opened and read without checking file permissions.
API secrets should be stored in files with mode `0600`. Reading a world-readable
credentials file without warning is a security gap.

---

### M4. `getenv("HOME")` is not thread-safe (POSIX)

**File:** `kraken_rest_api.hpp:172`

`getenv()` is not guaranteed to be thread-safe by POSIX if any thread calls `setenv()`
or `putenv()`. The test in `test_client.cpp:162-165` actively calls `unsetenv("HOME")`
which mutates the environment — this is a data race if any other test or thread
accesses the environment concurrently.

---

### M5. curl handle state leaks between requests

**File:** `kraken_rest_client.cpp:55-91`

The curl handle is reused across requests but `curl_easy_setopt` calls accumulate.
For example, a POST request sets `CURLOPT_POST` and `CURLOPT_POSTFIELDS`. A
subsequent GET request sets `CURLOPT_HTTPGET`, which should reset POST mode — but
other options like `CURLOPT_HTTPHEADER` from the previous request persist unless
explicitly cleared.

**Impact:** Headers or settings from a previous request could leak into the next.

**Fix:** Call `curl_easy_reset(curl_)` at the top of `curl_perform`, or set all
options explicitly for each request (including clearing headers).

---

### M6. Duplicate subscriptions silently overwrite

**File:** `kraken_ws_client.inl:160-161`

```cpp
subscriptions_[ch] = *erased_push;
```

If the user subscribes to the same channel twice, the second subscription silently
overwrites the first callback. The first `SubscriptionHandle` becomes a dangling
reference — its `cancel()` will remove the second subscription.

**Impact:** Confusing behaviour; the first subscription's callback is silently dropped.

**Fix:** Either reject duplicate subscriptions or track multiple callbacks per channel.

---

### M7. Double JSON parse on every WebSocket message

**Files:** `kraken_ws_client.cpp:89-91`, `kraken_ws_client.inl:89-90`

Every incoming frame is parsed in `on_raw_message` to extract `req_id` / `channel`,
then the parsed `json` object is passed to the handler — but many `from_json()` methods
call `.at()` / `.get()` again, and `identify_message()` (if used externally) parses the
same frame again.

For the internal dispatch path this is a single parse (the `json` object is forwarded).
But `identify_message()` as a public API accepts `const json&` yet callers in examples
parse the raw string themselves first, resulting in a double parse for external users.

Not a correctness issue but worth documenting.

---

### M8. Missing HTTP response status code check

**File:** `kraken_rest_client.cpp:85-88`

After `curl_easy_perform`, only the curl error code is checked. The HTTP status code
is never inspected. A 500, 403, or 429 response will be treated as a successful
response and passed to the JSON parser. The JSON parser may then fail with an unhelpful
error if the body is HTML (e.g. Cloudflare error page).

**Fix:** Check `CURLINFO_RESPONSE_CODE` and return an error for non-2xx responses
with the raw body included in the error message.

---

## Low Issues

### L1. `make_nonce()` is not monotonic under clock adjustment

**File:** `kraken_rest_api.hpp:155-159`

`make_nonce()` uses `system_clock::now()`, which can go backwards (NTP adjustment,
DST, manual set). Kraken rejects nonces smaller than the last seen value. A clock
step-back produces "invalid nonce" errors.

**Fix:** Use a hybrid: `max(last_nonce + 1, system_clock_us)` with a thread-safe
atomic.

---

### L2. Legacy `kapi.cpp` BIO resource leak

**File:** `tests/examples/kapi.cpp:51-67`

`b64_decode` calls `BIO_free_all(bmem)` after `BIO_push(b64, bmem)` reassigns `bmem`.
The correct call should be `BIO_free_all(b64)` to free the entire chain from the top.
Same issue in `b64_encode` (lines 71-89).

This is legacy example code but shipped in the repo.

---

### L3. Example code missing JSON exception handling

**Files:** `tests/examples/private_ws.cpp:73`, `tests/examples/public_ws.cpp`

Raw WebSocket examples call `json::parse(msg->str)` without `try/catch`. A malformed
frame kills the callback and disconnects the WebSocket.

---

### L4. ~60% of REST endpoints lack unit tests

**File:** `tests/unit/test_rest_requests.cpp`, `tests/unit/test_rest_responses.cpp`

Only 14 of 35 REST request types have request-building and response-parsing tests.
Untested endpoints include: `AddOrderBatch`, `EditOrder`, `AmendOrder`,
`CancelAllOrders`, `CancelAllOrdersAfter`, `CancelOrderBatch`, `GetExtendedBalance`,
`GetTradeBalance`, `GetTradesHistory`, `QueryOrders`, `QueryTrades`,
`GetOpenPositions`, `GetLedgers`, `QueryLedgers`, `GetDepositMethods`,
`GetDepositAddresses`, `Withdraw`, `CancelWithdrawal`, `CreateSubaccount`,
`AllocateEarn`, `DeallocateEarn`.

---

## Recommended Priority

1. **C1 + C3** — Promise/pending handler issues. Fix together since they share the
   same code paths. Highest risk of user-visible hangs and memory leaks.
2. **H2 + H5 + M8 + M5** — REST client robustness. Timeout, thread-safety, curl state
   cleanup, and HTTP status checking are all in `kraken_rest_client.cpp`.
3. **C2 + H1** — WebSocket thread-safety. Fix `disconnect_cb_` race and document/fix
   `send()` concurrency.
4. **H4 + M1 + M2** — JSON parsing robustness. Unify patterns and guard against
   malformed responses.
5. **L4** — Test coverage. Add tests for untested endpoints.
