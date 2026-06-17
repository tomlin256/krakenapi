// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/kraken/ws_api.inl — template implementations for ws_api.hpp.
// Included at the bottom of ws_api.hpp; do not include directly.

#pragma once

namespace exchange::kraken::ws {

template<typename PushMsg, SubscribeChannel Ch>
TypedSubscribeRequest<PushMsg, Ch>::TypedSubscribeRequest() { this->channel = Ch; }

// Routing key used by ExchangeWsClient to dispatch incoming push frames.
template<typename PushMsg, SubscribeChannel Ch>
std::string TypedSubscribeRequest<PushMsg, Ch>::route_key() const {
    return to_string(channel);
}

// Pre-built UNSUBSCRIBE frame sent by SubscriptionHandle::cancel().
template<typename PushMsg, SubscribeChannel Ch>
json TypedSubscribeRequest<PushMsg, Ch>::unsubscribe_json() const {
    UnsubscribeRequest unsub;
    unsub.channel = channel;
    unsub.symbols = symbols;
    unsub.token   = token;
    return unsub.to_json();
}

} // namespace exchange::kraken::ws
