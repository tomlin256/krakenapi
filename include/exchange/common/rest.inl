// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/common/rest.inl — template method implementations for rest.hpp.
// Included at the bottom of rest.hpp; do not include directly.

#pragma once

namespace exchange::rest {

template<typename T>
bool RestResponse<T>::has_error() const { return !errors.empty(); }

template<typename T>
const std::string& RestResponse<T>::first_error() const {
    static const std::string none;
    return errors.empty() ? none : errors[0];
}

} // namespace exchange::rest
