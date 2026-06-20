// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/cryptocom/types.hpp"

#include <stdexcept>
#include <string>

namespace exchange::cryptocom {

// ── Side ─────────────────────────────────────────────────────────────────────

std::string cryptocom_side_to_string(Side v) {
    return v == Side::Buy ? "BUY" : "SELL";
}

Side cryptocom_side_from_string(const std::string& s) {
    if (s == "BUY")  return Side::Buy;
    if (s == "SELL") return Side::Sell;
    throw std::invalid_argument("Unknown Crypto.com side: " + s);
}

// ── OrderType ────────────────────────────────────────────────────────────────

std::string cryptocom_order_type_to_string(OrderType v) {
    switch (v) {
        case OrderType::Limit:           return "LIMIT";
        case OrderType::Market:          return "MARKET";
        case OrderType::StopLoss:        return "STOP_LOSS";
        case OrderType::StopLossLimit:   return "STOP_LIMIT";
        case OrderType::TakeProfit:      return "TAKE_PROFIT";
        case OrderType::TakeProfitLimit: return "TAKE_PROFIT_LIMIT";
        case OrderType::Iceberg:
        case OrderType::TrailingStop:
        case OrderType::TrailingStopLimit:
        case OrderType::SettlePosition:
            throw std::invalid_argument(
                "OrderType not supported by Crypto.com: " + exchange::to_string(v));
    }
    throw std::invalid_argument("Unknown OrderType");
}

OrderType cryptocom_order_type_from_string(const std::string& s) {
    if (s == "LIMIT")             return OrderType::Limit;
    if (s == "MARKET")            return OrderType::Market;
    if (s == "STOP_LOSS")         return OrderType::StopLoss;
    if (s == "STOP_LIMIT")        return OrderType::StopLossLimit;
    if (s == "TAKE_PROFIT")       return OrderType::TakeProfit;
    if (s == "TAKE_PROFIT_LIMIT") return OrderType::TakeProfitLimit;
    throw std::invalid_argument("Unmappable Crypto.com order type: " + s);
}

// ── TimeInForce ──────────────────────────────────────────────────────────────

std::string cryptocom_tif_to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "GOOD_TILL_CANCEL";
        case TimeInForce::IOC: return "IMMEDIATE_OR_CANCEL";
        case TimeInForce::FOK: return "FILL_OR_KILL";
        case TimeInForce::GTD:
            throw std::invalid_argument("TimeInForce GTD not supported by Crypto.com");
    }
    throw std::invalid_argument("Unknown TimeInForce");
}

TimeInForce cryptocom_tif_from_string(const std::string& s) {
    if (s == "GOOD_TILL_CANCEL")    return TimeInForce::GTC;
    if (s == "IMMEDIATE_OR_CANCEL") return TimeInForce::IOC;
    if (s == "FILL_OR_KILL")        return TimeInForce::FOK;
    throw std::invalid_argument("Unknown Crypto.com time-in-force: " + s);
}

// ── OrderStatus (total — Unknown fallback) ───────────────────────────────────

OrderStatus cryptocom_order_status_from_string(const std::string& s) {
    if (s == "ACTIVE")           return OrderStatus::New;
    if (s == "NEW")              return OrderStatus::New;
    if (s == "PENDING")          return OrderStatus::PendingNew;
    if (s == "PARTIALLY_FILLED") return OrderStatus::PartiallyFilled;
    if (s == "FILLED")           return OrderStatus::Filled;
    if (s == "CANCELED")         return OrderStatus::Canceled;
    if (s == "CANCELLED")        return OrderStatus::Canceled;
    if (s == "EXPIRED")          return OrderStatus::Expired;
    return OrderStatus::Unknown;  // REJECTED and any unmapped status (read raw)
}

} // namespace exchange::cryptocom
