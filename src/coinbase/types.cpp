// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/coinbase/types.hpp"

#include <stdexcept>
#include <string>

namespace exchange::coinbase {

// ── Side ─────────────────────────────────────────────────────────────────────

std::string coinbase_side_to_string(Side v) {
    return v == Side::Buy ? "buy" : "sell";
}

Side coinbase_side_from_string(const std::string& s) {
    if (s == "buy")  return Side::Buy;
    if (s == "sell") return Side::Sell;
    throw std::invalid_argument("Unknown Coinbase side: " + s);
}

// ── OrderType ────────────────────────────────────────────────────────────────

std::string coinbase_order_type_to_string(OrderType v) {
    switch (v) {
        case OrderType::Limit:  return "limit";
        case OrderType::Market: return "market";
        case OrderType::Iceberg:
        case OrderType::StopLoss:
        case OrderType::StopLossLimit:
        case OrderType::TakeProfit:
        case OrderType::TakeProfitLimit:
        case OrderType::TrailingStop:
        case OrderType::TrailingStopLimit:
        case OrderType::SettlePosition:
            throw std::invalid_argument(
                "OrderType not supported by Coinbase: " + exchange::to_string(v));
    }
    throw std::invalid_argument("Unknown OrderType");
}

OrderType coinbase_order_type_from_string(const std::string& s) {
    if (s == "limit")  return OrderType::Limit;
    if (s == "market") return OrderType::Market;
    throw std::invalid_argument("Unmappable Coinbase order type: " + s);
}

// ── TimeInForce ──────────────────────────────────────────────────────────────

std::string coinbase_tif_to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::GTD: return "GTT";  // Coinbase "good-til-time"
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
    }
    throw std::invalid_argument("Unknown TimeInForce");
}

TimeInForce coinbase_tif_from_string(const std::string& s) {
    if (s == "GTC") return TimeInForce::GTC;
    if (s == "GTT") return TimeInForce::GTD;
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    throw std::invalid_argument("Unknown Coinbase time-in-force: " + s);
}

// ── OrderStatus ──────────────────────────────────────────────────────────────

OrderStatus coinbase_order_status_from_string(const std::string& s) {
    if (s == "received")  return OrderStatus::PendingNew;
    if (s == "pending")   return OrderStatus::PendingNew;
    if (s == "open")      return OrderStatus::New;
    if (s == "active")    return OrderStatus::New;
    if (s == "done")      return OrderStatus::Filled;   // see header note (done_reason)
    if (s == "settled")   return OrderStatus::Filled;
    if (s == "canceled")  return OrderStatus::Canceled;
    if (s == "cancelled") return OrderStatus::Canceled;
    return OrderStatus::Unknown;
}

} // namespace exchange::coinbase
