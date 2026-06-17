// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/binance/ws_streams.inl — template implementations for ws_streams.hpp.
// Included at the bottom of ws_streams.hpp; do not include directly.

#pragma once

namespace exchange::binance::ws {

template<typename PushMsg>
std::string TypedStreamSubscribeRequest<PushMsg>::route_key() const {
    return stream;
}

template<typename PushMsg>
json TypedStreamSubscribeRequest<PushMsg>::to_json() const {
    return {{"method", "SUBSCRIBE"},
            {"params", json::array({stream})},
            {"id", req_id}};
}

// Pre-built by subscribe_async after req_id assignment; sent on cancel().
template<typename PushMsg>
json TypedStreamSubscribeRequest<PushMsg>::unsubscribe_json() const {
    return {{"method", "UNSUBSCRIBE"},
            {"params", json::array({stream})},
            {"id", req_id}};
}

} // namespace exchange::binance::ws
