// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/types.hpp"

#include <stdexcept>
#include <string>

namespace exchange::binance {

// ── Side ─────────────────────────────────────────────────────────────────────

std::string binance_side_to_string(Side v) {
    return v == Side::Buy ? "BUY" : "SELL";
}

Side binance_side_from_string(const std::string& s) {
    if (s == "BUY")  return Side::Buy;
    if (s == "SELL") return Side::Sell;
    throw std::invalid_argument("Unknown Binance side: " + s);
}

// ── OrderType ────────────────────────────────────────────────────────────────

std::string binance_order_type_to_string(OrderType v) {
    switch (v) {
        case OrderType::Limit:           return "LIMIT";
        case OrderType::Market:          return "MARKET";
        case OrderType::StopLoss:        return "STOP_LOSS";
        case OrderType::StopLossLimit:   return "STOP_LOSS_LIMIT";
        case OrderType::TakeProfit:      return "TAKE_PROFIT";
        case OrderType::TakeProfitLimit: return "TAKE_PROFIT_LIMIT";
        case OrderType::Iceberg:
        case OrderType::TrailingStop:
        case OrderType::TrailingStopLimit:
        case OrderType::SettlePosition:
            throw std::invalid_argument(
                "OrderType not supported by Binance: " + exchange::to_string(v));
    }
    throw std::invalid_argument("Unknown OrderType");
}

OrderType binance_order_type_from_string(const std::string& s) {
    if (s == "LIMIT")             return OrderType::Limit;
    if (s == "MARKET")            return OrderType::Market;
    if (s == "STOP_LOSS")         return OrderType::StopLoss;
    if (s == "STOP_LOSS_LIMIT")   return OrderType::StopLossLimit;
    if (s == "TAKE_PROFIT")       return OrderType::TakeProfit;
    if (s == "TAKE_PROFIT_LIMIT") return OrderType::TakeProfitLimit;
    throw std::invalid_argument("Unmappable Binance order type: " + s);
}

// ── TimeInForce ──────────────────────────────────────────────────────────────

std::string binance_tif_to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        case TimeInForce::GTD:
            throw std::invalid_argument("Binance does not support GTD time-in-force");
    }
    throw std::invalid_argument("Unknown TimeInForce");
}

TimeInForce binance_tif_from_string(const std::string& s) {
    if (s == "GTC") return TimeInForce::GTC;
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    throw std::invalid_argument("Unknown Binance time-in-force: " + s);
}

// ── OrderStatus ──────────────────────────────────────────────────────────────

OrderStatus binance_order_status_from_string(const std::string& s) {
    if (s == "NEW")              return OrderStatus::New;
    if (s == "PARTIALLY_FILLED") return OrderStatus::PartiallyFilled;
    if (s == "FILLED")           return OrderStatus::Filled;
    if (s == "CANCELED")         return OrderStatus::Canceled;
    if (s == "EXPIRED")          return OrderStatus::Expired;
    if (s == "EXPIRED_IN_MATCH") return OrderStatus::Expired;
    return OrderStatus::Unknown;
}

// ── newOrderRespType (POST /api/v3/order) ────────────────────────────────────

std::string binance_order_resp_type_to_string(BinanceOrderRespType v) {
    switch (v) {
        case BinanceOrderRespType::Ack:    return "ACK";
        case BinanceOrderRespType::Result: return "RESULT";
        case BinanceOrderRespType::Full:   return "FULL";
    }
    throw std::invalid_argument("Unknown BinanceOrderRespType");
}

// ── BinanceBookLevel ─────────────────────────────────────────────────────────

BinanceBookLevel BinanceBookLevel::from_json(const json& row) {
    BinanceBookLevel l;
    l.price    = std::stod(row.at(0).get<std::string>());
    l.quantity = std::stod(row.at(1).get<std::string>());
    return l;
}

} // namespace exchange::binance
