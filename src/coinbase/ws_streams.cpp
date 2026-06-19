// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/coinbase/ws_streams.hpp"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace exchange::coinbase::ws {

namespace {

// Coinbase WS numeric fields arrive as JSON strings ("63212.97").
double snum(const json& j, const char* key) {
    return std::stod(j.value(key, std::string{"0"}));
}

std::optional<double> opt_num(const json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_string())
        return std::stod(j.at(key).get<std::string>());
    return std::nullopt;
}

std::optional<std::string> opt_str(const json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_string())
        return j.at(key).get<std::string>();
    return std::nullopt;
}

std::string now_unix_seconds() {
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

} // namespace

// ── Event from_json ───────────────────────────────────────────────────────────

CoinbaseTickerEvent CoinbaseTickerEvent::from_json(const json& j) {
    CoinbaseTickerEvent e;
    e.product_id = j.value("product_id", "");
    e.sequence   = j.value("sequence", static_cast<int64_t>(0));
    e.trade_id   = j.value("trade_id", static_cast<int64_t>(0));
    e.price      = snum(j, "price");
    e.best_bid   = snum(j, "best_bid");
    e.best_ask   = snum(j, "best_ask");
    e.last_size  = snum(j, "last_size");
    e.volume_24h = snum(j, "volume_24h");
    e.side       = j.value("side", "");
    e.time       = j.value("time", "");
    return e;
}

CoinbaseMatchEvent CoinbaseMatchEvent::from_json(const json& j) {
    CoinbaseMatchEvent e;
    e.trade_id       = j.value("trade_id", static_cast<int64_t>(0));
    e.sequence       = j.value("sequence", static_cast<int64_t>(0));
    e.product_id     = j.value("product_id", "");
    e.size           = snum(j, "size");
    e.price          = snum(j, "price");
    e.side           = j.value("side", "");
    e.time           = j.value("time", "");
    e.maker_order_id = j.value("maker_order_id", "");
    e.taker_order_id = j.value("taker_order_id", "");
    return e;
}

CoinbaseL2Snapshot CoinbaseL2Snapshot::from_json(const json& j) {
    CoinbaseL2Snapshot s;
    s.product_id = j.value("product_id", "");
    if (j.contains("bids"))
        for (const auto& row : j.at("bids"))
            s.bids.push_back({std::stod(row.at(0).get<std::string>()),
                              std::stod(row.at(1).get<std::string>())});
    if (j.contains("asks"))
        for (const auto& row : j.at("asks"))
            s.asks.push_back({std::stod(row.at(0).get<std::string>()),
                              std::stod(row.at(1).get<std::string>())});
    return s;
}

CoinbaseL2Update CoinbaseL2Update::from_json(const json& j) {
    CoinbaseL2Update u;
    u.product_id = j.value("product_id", "");
    u.time       = j.value("time", "");
    if (j.contains("changes"))
        for (const auto& row : j.at("changes"))
            u.changes.push_back({row.at(0).get<std::string>(),
                                 std::stod(row.at(1).get<std::string>()),
                                 std::stod(row.at(2).get<std::string>())});
    return u;
}

CoinbaseHeartbeatEvent CoinbaseHeartbeatEvent::from_json(const json& j) {
    CoinbaseHeartbeatEvent e;
    e.sequence      = j.value("sequence", static_cast<int64_t>(0));
    e.last_trade_id = j.value("last_trade_id", static_cast<int64_t>(0));
    e.product_id    = j.value("product_id", "");
    e.time          = j.value("time", "");
    return e;
}

CoinbaseLifecycleEvent CoinbaseLifecycleEvent::from_json(const json& j) {
    CoinbaseLifecycleEvent e;
    e.type           = j.value("type", "");
    e.product_id     = j.value("product_id", "");
    e.sequence       = j.value("sequence", static_cast<int64_t>(0));
    e.order_id       = j.value("order_id", "");
    e.side           = j.value("side", "");
    // "activate" frames carry "timestamp" instead of "time".
    e.time           = j.contains("time") ? j.value("time", "") : j.value("timestamp", "");
    e.price          = opt_num(j, "price");
    e.size           = opt_num(j, "size");
    e.remaining_size = opt_num(j, "remaining_size");
    e.reason         = opt_str(j, "reason");
    if (j.contains("trade_id") && j.at("trade_id").is_number())
        e.trade_id = j.at("trade_id").get<int64_t>();
    e.maker_order_id = opt_str(j, "maker_order_id");
    e.taker_order_id = opt_str(j, "taker_order_id");
    e.raw            = j;
    return e;
}

CoinbaseErrorEvent CoinbaseErrorEvent::from_json(const json& j) {
    CoinbaseErrorEvent e;
    e.message = j.value("message", "");
    e.reason  = opt_str(j, "reason");
    e.raw     = j;
    return e;
}

// ── CoinbaseSubscriptionHandle ────────────────────────────────────────────────

bool CoinbaseSubscriptionHandle::is_active() const {
    return active_ && active_->load();
}

void CoinbaseSubscriptionHandle::cancel() {
    if (!active_) return;
    if (!active_->exchange(false)) return;  // already cancelled — idempotent
    if (auto c = client_.lock())
        c->remove_handlers(message_types_, unsub_json_);
}

// ── CoinbaseStreamClient ──────────────────────────────────────────────────────

CoinbaseStreamClient::CoinbaseStreamClient(std::shared_ptr<IWsConnection>   conn,
                                           std::shared_ptr<IWsErrorHandler> error_handler)
    : conn_(std::move(conn))
    , error_handler_(std::move(error_handler))
{}

void CoinbaseStreamClient::init() {
    auto self = weak_from_this();
    conn_->set_on_open([self] {
        if (auto c = self.lock()) c->on_open_handler();
    });
    conn_->set_on_message([self](const std::string& m) {
        if (auto c = self.lock()) c->on_raw_message(m);
    });
    conn_->set_on_close([self](const std::string&) {
        if (auto c = self.lock()) c->connected_.store(false);
    });
    conn_->set_on_error([self](const std::string& reason) {
        if (auto c = self.lock())
            if (c->error_handler_) c->error_handler_->on_connection_error(reason);
    });
}

void CoinbaseStreamClient::connect()    { conn_->connect();    }
void CoinbaseStreamClient::disconnect() { conn_->disconnect(); }

void CoinbaseStreamClient::set_on_error(ErrorCallback cb) {
    std::lock_guard<std::mutex> lk(mu_);
    on_error_ = std::move(cb);
}

void CoinbaseStreamClient::set_on_subscriptions(SubscriptionsCallback cb) {
    std::lock_guard<std::mutex> lk(mu_);
    on_subscriptions_ = std::move(cb);
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_raw(
    const std::string&              channel,
    const std::vector<std::string>& product_ids,
    const std::vector<std::string>& message_types,
    const std::function<void(const json&)>& raw_handler,
    const json&                     extra_top_level)
{
    json sub = extra_top_level;          // auth fields (user channel) or {}
    sub["type"]        = "subscribe";
    sub["product_ids"] = product_ids;
    sub["channels"]    = json::array({channel});

    json unsub;
    unsub["type"]        = "unsubscribe";
    unsub["product_ids"] = product_ids;
    unsub["channels"]    = json::array({channel});

    {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& t : message_types)
            handlers_[t] = raw_handler;
    }

    auto active = std::make_shared<std::atomic<bool>>(true);
    CoinbaseSubscriptionHandle handle(active, weak_from_this(), message_types, unsub.dump());

    enqueue_or_send(sub.dump());
    return handle;
}

void CoinbaseStreamClient::remove_handlers(const std::vector<std::string>& message_types,
                                           const std::string& unsub_json) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& t : message_types)
            handlers_.erase(t);
    }
    enqueue_or_send(unsub_json);
}

void CoinbaseStreamClient::on_open_handler() {
    std::vector<std::string> queued;
    {
        std::lock_guard<std::mutex> lk(mu_);
        connected_.store(true);
        queued.swap(send_queue_);
    }
    for (const auto& m : queued)
        conn_->send(m);
}

void CoinbaseStreamClient::enqueue_or_send(const std::string& payload) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!connected_.load()) {
            send_queue_.push_back(payload);
            return;
        }
    }
    conn_->send(payload);
}

void CoinbaseStreamClient::on_raw_message(const std::string& raw) {
    json j;
    try {
        j = json::parse(raw);
    } catch (const std::exception& e) {
        if (error_handler_) error_handler_->on_malformed_frame(raw, e);
        return;
    }

    const std::string type = j.value("type", "");

    if (type == "subscriptions") {
        SubscriptionsCallback cb;
        { std::lock_guard<std::mutex> lk(mu_); cb = on_subscriptions_; }
        if (cb) cb(j);
        return;
    }

    if (type == "error") {
        const CoinbaseErrorEvent ev = CoinbaseErrorEvent::from_json(j);
        ErrorCallback cb;
        { std::lock_guard<std::mutex> lk(mu_); cb = on_error_; }
        if (cb) cb(ev);
        if (error_handler_) error_handler_->on_connection_error(ev.message);
        return;
    }

    std::function<void(const json&)> handler;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = handlers_.find(type);
        if (it != handlers_.end()) handler = it->second;
    }
    if (handler) {
        // A malformed push frame must not throw into the receive thread.
        try {
            handler(j);
        } catch (const std::exception& e) {
            if (error_handler_) error_handler_->on_malformed_frame(raw, e);
        }
    }
}

// ── Typed subscriptions ───────────────────────────────────────────────────────

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_ticker(
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseTickerEvent)> cb)
{
    return subscribe_raw("ticker", product_ids, {"ticker"},
        [cb = std::move(cb)](const json& j) { cb(CoinbaseTickerEvent::from_json(j)); });
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_matches(
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseMatchEvent)> cb)
{
    return subscribe_raw("matches", product_ids, {"match", "last_match"},
        [cb = std::move(cb)](const json& j) { cb(CoinbaseMatchEvent::from_json(j)); });
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_level2(
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseL2Snapshot)> on_snapshot,
    std::function<void(CoinbaseL2Update)>   on_update)
{
    return subscribe_raw("level2", product_ids, {"snapshot", "l2update"},
        [on_snapshot = std::move(on_snapshot),
         on_update   = std::move(on_update)](const json& j) {
            if (j.value("type", "") == "snapshot")
                on_snapshot(CoinbaseL2Snapshot::from_json(j));
            else
                on_update(CoinbaseL2Update::from_json(j));
        });
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_heartbeat(
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseHeartbeatEvent)> cb)
{
    return subscribe_raw("heartbeat", product_ids, {"heartbeat"},
        [cb = std::move(cb)](const json& j) { cb(CoinbaseHeartbeatEvent::from_json(j)); });
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_full(
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseLifecycleEvent)> cb)
{
    return subscribe_raw("full", product_ids,
        {"received", "open", "done", "match", "change", "activate"},
        [cb = std::move(cb)](const json& j) { cb(CoinbaseLifecycleEvent::from_json(j)); });
}

CoinbaseSubscriptionHandle CoinbaseStreamClient::subscribe_user(
    const CoinbaseWsCredentials&    creds,
    const std::vector<std::string>& product_ids,
    std::function<void(CoinbaseLifecycleEvent)> cb)
{
    const std::string ts  = now_unix_seconds();
    const std::string sig = exchange::coinbase::rest::detail::coinbase_sign(
        creds.api_secret, ts, "GET", "/users/self/verify", "");

    json auth;
    auth["signature"]  = sig;
    auth["key"]        = creds.api_key;
    auth["passphrase"] = creds.passphrase;
    auth["timestamp"]  = ts;

    return subscribe_raw("user", product_ids,
        {"received", "open", "done", "match", "change", "activate"},
        [cb = std::move(cb)](const json& j) { cb(CoinbaseLifecycleEvent::from_json(j)); },
        auth);
}

// ── Factory ───────────────────────────────────────────────────────────────────

std::shared_ptr<CoinbaseStreamClient>
make_coinbase_stream_client(std::shared_ptr<IWsConnection>   conn,
                            std::shared_ptr<IWsErrorHandler> error_handler) {
    auto client = std::make_shared<CoinbaseStreamClient>(
        std::move(conn), std::move(error_handler));
    client->init();
    return client;
}

} // namespace exchange::coinbase::ws
