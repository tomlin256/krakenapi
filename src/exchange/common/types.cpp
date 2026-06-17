// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/common/types.hpp"

#include <stdexcept>

namespace exchange {

std::string to_string(Side v) { return v == Side::Buy ? "buy" : "sell"; }

Side side_from_string(const std::string& s) {
    if (s == "buy")  return Side::Buy;
    if (s == "sell") return Side::Sell;
    throw std::invalid_argument("Unknown side: " + s);
}

std::string to_string(OrderType v) {
    switch (v) {
        case OrderType::Limit:             return "limit";
        case OrderType::Market:            return "market";
        case OrderType::Iceberg:           return "iceberg";
        case OrderType::StopLoss:          return "stop_loss";
        case OrderType::StopLossLimit:     return "stop_loss_limit";
        case OrderType::TakeProfit:        return "take_profit";
        case OrderType::TakeProfitLimit:   return "take_profit_limit";
        case OrderType::TrailingStop:      return "trailing_stop";
        case OrderType::TrailingStopLimit: return "trailing_stop_limit";
        case OrderType::SettlePosition:    return "settle_position";
    }
    throw std::invalid_argument("Unknown OrderType");
}

std::string to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "gtc";
        case TimeInForce::GTD: return "gtd";
        case TimeInForce::IOC: return "ioc";
        case TimeInForce::FOK: return "fok";
    }
    throw std::invalid_argument("Unknown TimeInForce");
}

std::string to_string(OrderStatus v) {
    switch (v) {
        case OrderStatus::PendingNew:      return "pending_new";
        case OrderStatus::New:             return "new";
        case OrderStatus::PartiallyFilled: return "partially_filled";
        case OrderStatus::Filled:          return "filled";
        case OrderStatus::Canceled:        return "canceled";
        case OrderStatus::Expired:         return "expired";
        case OrderStatus::Unknown:         return "unknown";
    }
    return "unknown";
}

OrderStatus order_status_from_string(const std::string& s) {
    if (s == "pending_new" || s == "pending") return OrderStatus::PendingNew;
    if (s == "new" || s == "open")            return OrderStatus::New;
    if (s == "partially_filled")              return OrderStatus::PartiallyFilled;
    if (s == "filled" || s == "closed")       return OrderStatus::Filled;
    if (s == "canceled" || s == "cancelled")  return OrderStatus::Canceled;
    if (s == "expired")                       return OrderStatus::Expired;
    return OrderStatus::Unknown;
}

} // namespace exchange
