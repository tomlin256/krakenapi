# WebSocket Error Handling Gaps

Generated: 2026-03-23

---

## Background

Discovered during a live Flywheel engine session (2026-03-23) in which the engine stopped
ticking at ~14:25 with no log output. Investigation traced the silence to three places where
WebSocket errors and disconnects can occur without any notification reaching the caller.

---

## Gap 1 — `IxWsConnection` drops `ix::WebSocketMessageType::Error` frames

**File:** `include/kraken_ix_ws_connection.hpp`

**Severity:** High

### Problem

The `setOnMessageCallback` switch in `IxWsConnection::connect()` handles `Open`, `Close`, and
`Message`, but falls through to `default: break` for `ix::WebSocketMessageType::Error`:

```cpp
switch (msg->type) {
    case ix::WebSocketMessageType::Open:
        if (open_cb_)  open_cb_();
        break;
    case ix::WebSocketMessageType::Close:
        if (close_cb_) close_cb_();
        break;
    case ix::WebSocketMessageType::Message:
        if (msg_cb_)   msg_cb_(msg->str);
        break;
    default:
        break;  // ← Error frames land here and are silently swallowed
}
```

ixwebsocket fires `Error` for TLS handshake failures, connection resets, protocol violations,
and DNS resolution failures. All of these are invisible to the caller.

### Fix

1. Add `ErrorCb = std::function<void(const std::string& reason)>` to `IWsConnection` alongside
   the existing `MessageCb`, `OpenCb`, and `CloseCb`.
2. Add `set_on_error(ErrorCb cb)` to the `IWsConnection` interface.
3. Handle the case in `IxWsConnection`:
   ```cpp
   case ix::WebSocketMessageType::Error:
       if (error_cb_) error_cb_(msg->errorInfo.reason);
       break;
   ```
4. In `KrakenWsClient::init()`, register a default error handler that logs via
   `error_handler_` (or a fallback `RateLimitedWsErrorHandler`).

---

## Gap 2 — `KrakenWsClient` close notification never reaches callers

**File:** `src/kraken_ws_client.cpp`

**Severity:** High

### Problem

`KrakenWsClient::init()` registers a close callback that only sets an internal flag:

```cpp
conn_->set_on_close([weak_self]() {
    if (auto self = weak_self.lock()) self->connected_.store(false);
});
```

There is no way for a caller (e.g. `LiveKrakenFeed`) to be notified when the socket closes.
When the Kraken server drops the connection, `connected_` becomes `false` and ticks silently
stop — the feed thread exits cleanly with no warning and no engine wake.

The `LiveKrakenFeed` header comment even acknowledges the missing method:
> "If the WebSocket disconnects, `onDisconnected()` is called and the feed stops posting."

But `onDisconnected()` does not exist in the class.

### Fix

1. Add `set_on_disconnect(std::function<void()> cb)` to `KrakenWsClient` (public).
2. Store the callback and call it from the existing close handler in `init()`:
   ```cpp
   conn_->set_on_close([weak_self]() {
       if (auto self = weak_self.lock()) {
           self->connected_.store(false);
           if (self->disconnect_cb_) self->disconnect_cb_();
       }
   });
   ```
3. In `LiveKrakenFeed::start()`, register a disconnect callback:
   ```cpp
   client_->set_on_disconnect([]() {
       spdlog::error("[live] WebSocket disconnected — feed has stopped");
   });
   ```

---

## Gap 3 — Malformed frame errors log to `stderr`, not `spdlog`

**File:** `include/flywheel/kraken_live_feed.inl` (Flywheel consumer)

**Severity:** Medium

### Problem

`LiveKrakenFeed::start()` calls `make_ws_client` with no error handler:

```cpp
client_ = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));
```

`KrakenWsClient::init()` installs a `RateLimitedWsErrorHandler` when `nullptr` is passed, but
that handler uses `std::fprintf(stderr, ...)`. In deployments where stderr is not captured,
malformed-frame warnings are silently lost.

### Fix

Callers that use `spdlog` should pass a custom `IWsErrorHandler` that routes through spdlog:

```cpp
class SpdlogWsErrorHandler : public kraken::ws::IWsErrorHandler {
public:
    void on_malformed_frame(const std::string& raw,
                            const std::exception& e) override {
        spdlog::warn("[ws] malformed frame ({}): {:.120}", e.what(), raw);
    }
};
```

Then in `LiveKrakenFeed::start()`:

```cpp
client_ = kraken::ws::make_ws_client(
    std::string(kraken::ws::PUBLIC_WS_URL),
    std::make_shared<SpdlogWsErrorHandler>());
```

Note: this fix lives in the Flywheel repo, not krakenapi. It is listed here because the
root cause is `RateLimitedWsErrorHandler` writing to `stderr` rather than accepting an
injectable sink.

---

## Summary

| Gap | File | Severity | Change required |
|-----|------|----------|-----------------|
| `Error` frame dropped | `kraken_ix_ws_connection.hpp` | High | Add `ErrorCb` to `IWsConnection`; handle in `IxWsConnection` |
| No disconnect callback | `kraken_ws_client.cpp` / `.hpp` | High | Add `set_on_disconnect()` to `KrakenWsClient` |
| Malformed frames → stderr | Consumer (Flywheel) | Medium | Pass spdlog-backed `IWsErrorHandler` to `make_ws_client` |
