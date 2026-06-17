// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/kraken/types.inl — template implementations for kraken/types.hpp.
// Included at the bottom of kraken/types.hpp; do not include directly.

#pragma once

namespace exchange::kraken {

template<typename T>
bool RestResponse<T>::has_error() const { return !errors.empty(); }

template<typename T>
const std::string& RestResponse<T>::first_error() const {
    static const std::string none;
    return errors.empty() ? none : errors[0];
}

// Helper: parse the outer envelope and call T::from_json(result_node)
template<typename T>
RestResponse<T> parse_rest_response(const json& j) {
    RestResponse<T> r;
    if (j.contains("error")) {
        for (const auto& e : j["error"])
            r.errors.push_back(e.get<std::string>());
    }
    r.ok = r.errors.empty();
    if (r.ok && j.contains("result"))
        r.result = T::from_json(j["result"]);
    return r;
}

} // namespace exchange::kraken
