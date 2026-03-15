# Code Review Action Plan

Generated: 2026-03-13

---

## Summary

| Severity | Count | Key Files |
|---|---|---|
| Critical | 3 | `kraken_types.hpp`, `kraken_rest_api.hpp` |
| High | 4 | `kraken_rest_client.cpp`, `kraken_ws_client.cpp`, `kraken_ws_api.hpp` |
| Design | 5 | `kraken_ws_client.hpp`, `kraken_ix_ws_connection.hpp`, `kraken_rest_client.hpp` |
| Security | 1 | `kraken_rest_api.hpp` |
| Test gaps | 6 | `test_rest_responses.cpp`, `test_client.cpp`, `test_ws_client.cpp` |

---

## Critical Bugs

### 1. Broken `stp_type` parsing
- **File:** `include/kraken_types.hpp:336`
- **Status:** [x] DONE

`stp_type` is hardcoded to `StpType::CancelNewest` regardless of the actual JSON value. Every order with any `stp_type` silently gets the wrong value.

```cpp
// Current (wrong):
if (j.contains("stp_type")) p.stp_type = StpType::CancelNewest; // parse if needed

// Fix:
if (j.contains("stp_type")) p.stp_type = stp_type_from_string(j["stp_type"].get<std::string>());
```

---

### 2. Missing `fee_preference_from_string` / fragile deserialisation
- **File:** `include/kraken_types.hpp:335`
- **Status:** [ ] TODO

Only `"base"` is handled; any other value silently becomes `Quote`. The `fee_preference_from_string()` free function (consistent with every other enum converter) is missing entirely.

```cpp
// Current — silent wrong default:
p.fee_preference = (j["fee_preference"].get<std::string>() == "base")
    ? FeePreference::Base : FeePreference::Quote;

// Fix: add fee_preference_from_string() and call it here
```

---

### 3. Null dereference on `getenv("HOME")`
- **File:** `include/kraken_rest_api.hpp:170`
- **Status:** [ ] TODO

Crashes in containers or CI environments where `HOME` is unset.

```cpp
// Current — undefined behaviour when HOME is unset:
std::string dir = location.empty()
    ? std::string(getenv("HOME")) + "/.kraken"
    : location;

// Fix:
const char* home = getenv("HOME");
if (!home) throw std::runtime_error("HOME environment variable not set");
std::string dir = location.empty() ? std::string(home) + "/.kraken" : location;
```

---

## High-Severity Issues

### 4. Silent discard of malformed WebSocket frames
- **File:** `src/kraken_ws_client.cpp:77`
- **Status:** [ ] TODO

Bare `catch(...)` with a silent `return` makes malformed-frame bugs invisible in production.

```cpp
// Current:
try { j = json::parse(raw); } catch (...) { return; }

// Fix: log at warn level before returning
try { j = json::parse(raw); }
catch (const std::exception& e) {
    spdlog::warn("KrakenWsClient: failed to parse message: {} | raw: {}", e.what(), raw);
    return;
}
```

---

### 5. Duplicate `Triggers` / `Conditional` struct definitions
- **Files:** `include/kraken_types.hpp` and `include/kraken_ws_api.hpp:43-63`
- **Status:** [ ] TODO

Both structs are defined twice. The copy in `kraken_ws_api.hpp` is a legacy artefact and should be removed so only the `kraken::` namespace versions are used.

---

### 6. No RAII for `curl_slist`
- **File:** `src/kraken_rest_client.cpp:64-86`
- **Status:** [ ] TODO

`curl_slist_free_all` is not called if an exception is thrown between allocation and cleanup. Wrap with a custom deleter:

```cpp
auto chunk_deleter = [](curl_slist* p) { if (p) curl_slist_free_all(p); };
std::unique_ptr<curl_slist, decltype(chunk_deleter)> chunk(nullptr, chunk_deleter);
```

---

### 7. Sign-extension bug in `url_encode`
- **File:** `include/kraken_rest_api.hpp:128`
- **Status:** [ ] TODO

C-style cast from `char` to `int` causes undefined behaviour on platforms where `char` is signed.

```cpp
// Current:
oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;

// Fix:
oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
    << static_cast<unsigned int>(static_cast<unsigned char>(c));
```

---

## Design / Architecture Issues

### 8. `make_test_client` exposed in the production header
- **File:** `include/kraken_rest_client.hpp:68-84`
- **Status:** [ ] TODO

The friend declaration and inline test factory are visible to all consumers. Move `make_test_client` to a separate `include/kraken_rest_client_test_utils.hpp` that is only included by tests.

---

### 9. Missing explicit copy/move deletion on `KrakenWsClient`
- **File:** `include/kraken_ws_client.hpp:162-166`
- **Status:** [ ] TODO

`KrakenWsClient` inherits `enable_shared_from_this` and holds a mutex; it must only live inside a `shared_ptr`. Make the intent explicit:

```cpp
KrakenWsClient(const KrakenWsClient&) = delete;
KrakenWsClient& operator=(const KrakenWsClient&) = delete;
```

---

### 10. Inconsistent error-handling paradigm
- **Files:** across REST and WS layers
- **Status:** [ ] TODO

Three different strategies are used: exceptions (`Credentials::from_file`), `RestResponse::ok`, and `WsResponse::ok`. Document the contract for each layer in CLAUDE.md so callers know what to expect.

---

### 11. No connection timeout for WebSocket
- **File:** `include/kraken_ix_ws_connection.hpp:38-55`
- **Status:** [ ] TODO

If the server is unreachable, `on_open` never fires. Callers blocked on `execute()` wait silently until the 5-second execute timeout expires with no indication of why. Add a connect-phase timeout or an explicit error callback path.

---

### 12. Non-monotonic nonce clock
- **File:** `include/kraken_rest_api.hpp:154-158`
- **Status:** [ ] TODO

`std::chrono::system_clock` can jump backwards on NTP adjustments, causing "invalid nonce" errors from the exchange. Consider a hybrid approach using `steady_clock` for the increment, or at minimum document the risk clearly.

---

## Security Issues

### 13. Credential file permissions not checked
- **File:** `include/kraken_rest_api.hpp:175`
- **Status:** [ ] TODO

`~/.kraken/default` is opened with no check that it is owner-read-only (`0600`). Add a `stat()` call and warn or throw if the file is group- or world-readable.

---

## Missing Test Coverage

### 14. `stp_type` and `fee_preference` parsing are untested
- **File:** `tests/unit/test_rest_responses.cpp`
- **Status:** [ ] TODO

Add tests for:
- each valid `stp_type` value round-tripping correctly
- each valid `fee_preference` value round-tripping correctly
- invalid enum values triggering `std::invalid_argument`

---

### 15. No error-path tests for `Credentials::from_file`
- **File:** `tests/unit/test_client.cpp`
- **Status:** [ ] TODO

Add tests for:
- file not found → `std::runtime_error`
- file with only one line (missing private key)
- `HOME` environment variable unset

---

### 16. No test for pre-connection queue flush
- **File:** `tests/unit/test_ws_client.cpp`
- **Status:** [ ] TODO

Add a test that:
1. Calls `execute()` or `subscribe()` *before* `fire_open()`
2. Asserts the message is **not** in `sent_messages` yet
3. Calls `fire_open()`
4. Asserts the message is now flushed to `sent_messages`

---

### 17. No concurrent-cancel deadlock test
- **File:** `tests/unit/test_ws_client.cpp`
- **Status:** [ ] TODO

`SubscriptionHandle::cancel()` is documented as safe to call from any thread including from within the push callback. Add a test that calls `cancel()` from inside the callback and verifies no deadlock occurs.

---

### 18. No parametrised enum round-trip tests
- **Files:** `tests/unit/test_rest_responses.cpp`, `tests/unit/test_rest_requests.cpp`
- **Status:** [ ] TODO

Replace hand-picked enum spot-checks with `TEST_P` / `INSTANTIATE_TEST_SUITE_P` tables covering every enum value. This ensures future additions that forget to update the converter are caught.

---

### 19. Weak assertions in response parsing tests
- **File:** `tests/unit/test_rest_responses.cpp`
- **Status:** [ ] TODO

Several tests only verify that parsing does not crash. Update them to assert actual field values against realistic JSON fixtures (e.g. assert `bid`, `ask`, `last` after parsing a `TickerResult`).

---

## Recommended Priority Order

1. [x] Fix `stp_type` parsing — data corruption on live orders
2. [ ] Add `fee_preference_from_string` — data corruption on live orders
3. [ ] Guard `getenv("HOME")` against null — crash in CI/containers
4. [ ] Log on JSON parse failure — production observability
5. [ ] Wrap `curl_slist` in RAII — resource safety
6. [ ] Fix `url_encode` sign-extension — correctness / UB
7. [ ] Add tests for untested enum paths and error paths
8. [ ] Remove duplicate `Triggers`/`Conditional` from `kraken_ws_api.hpp`
9. [ ] Move `make_test_client` to a test-only header
10. [ ] Add `= delete` copy/move on `KrakenWsClient`
11. [ ] Add credential file permission check
12. [ ] Add WebSocket connection timeout
13. [ ] Document error-handling conventions in CLAUDE.md
