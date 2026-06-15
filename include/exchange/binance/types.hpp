// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/types.hpp
// Binance-specific enum converters and small shared types.
//
// Namespace: exchange::binance
//
// The canonical exchange-agnostic enums (Side, OrderType, TimeInForce,
// OrderStatus) live in exchange/common/types.hpp and are re-exported here.
// Binance's wire formats are UPPERCASE ("BUY", "STOP_LOSS_LIMIT", "GTC") —
// diverging from the canonical lowercase strings — so, per the
// kraken_order_type_to_string precedent, Binance keeps its own converter
// pair per enum alongside (not instead of) the canonical converters.
//
// Conversion policy (plan 004 decision 3 — "enum where the wire↔enum
// mapping is total, raw string otherwise"):
//   - Side          : total both directions.
//   - TimeInForce   : total — Binance's set is exactly {GTC, IOC, FOK};
//                     GTD throws (no Binance wire format).
//   - OrderStatus   : from_string is total via the Unknown fallback.
//   - OrderType     : NOT total — Binance's LIMIT_MAKER (and any future
//                     type) has no canonical value, so response structs
//                     keep `type` as a raw string and these converters
//                     throw on unmappable input.

#include "exchange/common/types.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace exchange::binance {

using json = nlohmann::json;

// Re-export the canonical enums so exchange::binance code refers to them
// unprefixed (mirrors exchange/kraken/types.hpp).
using exchange::Side;
using exchange::OrderType;
using exchange::TimeInForce;
using exchange::OrderStatus;

// ── Side ─────────────────────────────────────────────────────────────────────

inline std::string binance_side_to_string(Side v) {
    return v == Side::Buy ? "BUY" : "SELL";
}

inline Side binance_side_from_string(const std::string& s) {
    if (s == "BUY")  return Side::Buy;
    if (s == "SELL") return Side::Sell;
    throw std::invalid_argument("Unknown Binance side: " + s);
}

// ── OrderType ────────────────────────────────────────────────────────────────
//
// Supports the 6 canonical types Binance accepts. Throws std::invalid_argument
// for canonical values with no Binance wire format (Iceberg, TrailingStop,
// TrailingStopLimit, SettlePosition) and, in from_string, for "LIMIT_MAKER"
// or unknown input — canonical OrderType has no Unknown value to absorb them.

inline std::string binance_order_type_to_string(OrderType v) {
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

inline OrderType binance_order_type_from_string(const std::string& s) {
    if (s == "LIMIT")             return OrderType::Limit;
    if (s == "MARKET")            return OrderType::Market;
    if (s == "STOP_LOSS")         return OrderType::StopLoss;
    if (s == "STOP_LOSS_LIMIT")   return OrderType::StopLossLimit;
    if (s == "TAKE_PROFIT")       return OrderType::TakeProfit;
    if (s == "TAKE_PROFIT_LIMIT") return OrderType::TakeProfitLimit;
    throw std::invalid_argument("Unmappable Binance order type: " + s);
}

// ── TimeInForce ──────────────────────────────────────────────────────────────

inline std::string binance_tif_to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        case TimeInForce::GTD:
            throw std::invalid_argument("Binance does not support GTD time-in-force");
    }
    throw std::invalid_argument("Unknown TimeInForce");
}

inline TimeInForce binance_tif_from_string(const std::string& s) {
    if (s == "GTC") return TimeInForce::GTC;
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    throw std::invalid_argument("Unknown Binance time-in-force: " + s);
}

// ── OrderStatus ──────────────────────────────────────────────────────────────
//
// Total — never throws. Unmapped wire statuses (PENDING_CANCEL, REJECTED,
// and anything Binance adds later) fold to OrderStatus::Unknown so a single
// odd row cannot blow up an entire openOrders/allOrders parse.

inline OrderStatus binance_order_status_from_string(const std::string& s) {
    if (s == "NEW")              return OrderStatus::New;
    if (s == "PARTIALLY_FILLED") return OrderStatus::PartiallyFilled;
    if (s == "FILLED")           return OrderStatus::Filled;
    if (s == "CANCELED")         return OrderStatus::Canceled;
    if (s == "EXPIRED")          return OrderStatus::Expired;
    if (s == "EXPIRED_IN_MATCH") return OrderStatus::Expired;
    return OrderStatus::Unknown;
}

// ── newOrderRespType (POST /api/v3/order) ────────────────────────────────────

enum class BinanceOrderRespType { Ack, Result, Full };

inline std::string binance_order_resp_type_to_string(BinanceOrderRespType v) {
    switch (v) {
        case BinanceOrderRespType::Ack:    return "ACK";
        case BinanceOrderRespType::Result: return "RESULT";
        case BinanceOrderRespType::Full:   return "FULL";
    }
    throw std::invalid_argument("Unknown BinanceOrderRespType");
}

// ── BinanceBookLevel ─────────────────────────────────────────────────────────
//
// One order-book price level. Shared by the REST order book
// (GET /api/v3/depth) and the WS depth streams (@depth diff and
// @depth<levels> partial), which all use the same positional row format.

struct BinanceBookLevel {
    double price{0.0};
    double quantity{0.0};
    // Parses a positional 2-element row: ["price","qty"].
    static BinanceBookLevel from_json(const json& row) {
        BinanceBookLevel l;
        l.price    = std::stod(row.at(0).get<std::string>());
        l.quantity = std::stod(row.at(1).get<std::string>());
        return l;
    }
};

} // namespace exchange::binance
