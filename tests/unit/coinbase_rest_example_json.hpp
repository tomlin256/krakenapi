// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Captured Coinbase Exchange public REST responses, used as deterministic
// fixtures by test_coinbase_rest_responses.cpp. The single-object samples
// (time, product, ticker, stats, book level 1, trades, candles) are verbatim
// live captures from api.exchange.coinbase.com (2026-06-19). The two
// multi-element samples are assembled from live captures: PRODUCTS_JSON wraps
// two real product objects; BOOK_L2_JSON is a trimmed level-2 book (same row
// shape as the live response). Synthetic portions are noted inline.

#pragma once

namespace coinbase_fixtures {

// GET /time
inline constexpr const char* TIME_JSON =
    R"({"iso":"2026-06-19T16:43:33.188Z","epoch":1781887413.188})";

// GET /products/BTC-USD  (verbatim live capture)
inline constexpr const char* PRODUCT_JSON =
    R"({"id":"BTC-USD","base_currency":"BTC","quote_currency":"USD",)"
    R"("quote_increment":"0.01","base_increment":"0.00000001","display_name":"BTC-USD",)"
    R"("min_market_funds":"1","margin_enabled":false,"post_only":false,"limit_only":false,)"
    R"("cancel_only":false,"status":"online","status_message":"","trading_disabled":false,)"
    R"("fx_stablecoin":false,"max_slippage_percentage":"0.02000000","auction_mode":false,)"
    R"("high_bid_limit_percentage":""})";

// GET /products  — array of two real-shaped product objects (BTC-USD verbatim;
// ETH-USD same shape, second element synthetic for a multi-row parse check).
inline constexpr const char* PRODUCTS_JSON =
    R"([{"id":"BTC-USD","base_currency":"BTC","quote_currency":"USD",)"
    R"("quote_increment":"0.01","base_increment":"0.00000001","display_name":"BTC-USD",)"
    R"("status":"online","trading_disabled":false},)"
    R"({"id":"ETH-USD","base_currency":"ETH","quote_currency":"USD",)"
    R"("quote_increment":"0.01","base_increment":"0.00000001","display_name":"ETH-USD",)"
    R"("status":"online","trading_disabled":false}])";

// GET /products/BTC-USD/book?level=1  (verbatim live capture)
inline constexpr const char* BOOK_L1_JSON =
    R"({"bids":[["63217.42","0.00004377",1]],"asks":[["63217.43","0.24090999",8]],)"
    R"("sequence":130929351062,"auction_mode":false,"auction":null,)"
    R"("time":"2026-06-19T16:43:31.844642586Z"})";

// GET /products/BTC-USD/book?level=2  — trimmed to 2 levels per side (synthetic
// trim; same ["price","size",num_orders] row shape as the live level-2 book).
inline constexpr const char* BOOK_L2_JSON =
    R"({"bids":[["63210.11","1.5",3],["63210.10","0.2",1]],)"
    R"("asks":[["63210.50","0.8",2],["63210.75","2.1",5]],)"
    R"("sequence":130929351099})";

// GET /products/BTC-USD/ticker  (verbatim live capture)
inline constexpr const char* TICKER_JSON =
    R"({"ask":"63212.98","bid":"63212.97","volume":"4916.94797396",)"
    R"("trade_id":1040406588,"price":"63212.97","size":"0.00782662",)"
    R"("time":"2026-06-19T16:43:33.016274911Z","rfq_volume":"31.625279"})";

// GET /products/BTC-USD/trades?limit=2  (verbatim live capture)
inline constexpr const char* TRADES_JSON =
    R"([{"trade_id":1040406590,"side":"buy","size":"0.00003219",)"
    R"("price":"63212.97000000","time":"2026-06-19T16:43:33.739704Z"},)"
    R"({"trade_id":1040406589,"side":"sell","size":"0.00195786",)"
    R"("price":"63212.98000000","time":"2026-06-19T16:43:33.366632Z"}])";

// GET /products/BTC-USD/candles?granularity=60  — first two rows (verbatim).
// Row shape: [time, low, high, open, close, volume].
inline constexpr const char* CANDLES_JSON =
    R"([[1781887140,63234.99,63244.78,63244.78,63239.75,0.69450254],)"
    R"([1781887080,63236.17,63262.46,63236.18,63244.77,0.89694057]])";

// GET /products/BTC-USD/stats  (verbatim live capture)
inline constexpr const char* STATS_JSON =
    R"({"open":"62605.56","high":"63351.3","low":"62159.76","last":"63212.41",)"
    R"("volume":"4918.33722403","volume_30day":"269980.64976703",)"
    R"("rfq_volume_24hour":"31.625279","rfq_volume_30day":"2310.097200"})";

} // namespace coinbase_fixtures
