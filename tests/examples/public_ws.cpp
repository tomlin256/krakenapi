// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include <ixwebsocket/IXWebSocket.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "kraken_ws_api.hpp"

#include <iostream>
#include <string>

static void print_usage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <channel> [<symbol>]\n\n"
              << "Available channels:\n"
              << "  ticker     - Level 1 best-bid/ask quotes  (symbol required, e.g. BTC/USD)\n"
              << "  book       - Level 2 order book           (symbol required)\n"
              << "  level3     - Level 3 order book           (symbol required)\n"
              << "  trade      - Recent trades                (symbol required)\n"
              << "  ohlc       - OHLC candles (1-min)         (symbol required)\n"
              << "  instrument - Instrument metadata          (no symbol required)\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string channel = argv[1];
    std::string symbol;

    // Channels that require a symbol argument
    bool needs_symbol = (channel == "ticker" || channel == "book" ||
                         channel == "level3" || channel == "trade" ||
                         channel == "ohlc");

    if (needs_symbol)
    {
        if (argc < 3)
        {
            std::cerr << "Channel '" << channel << "' requires a symbol argument.\n\n";
            print_usage(argv[0]);
            return 1;
        }
        symbol = argv[2];
    }

    // Validate channel name
    if (channel != "ticker" && channel != "book" && channel != "level3" &&
        channel != "trade" && channel != "ohlc" && channel != "instrument")
    {
        std::cerr << "Unknown channel: " << channel << "\n\n";
        print_usage(argv[0]);
        return 1;
    }

    ix::WebSocket webSocket;
    webSocket.setUrl("wss://ws.kraken.com/v2");

    webSocket.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                spdlog::info("ws opened");

                kraken::ws::SubscribeRequest req;

                if (channel == "ticker")
                {
                    req.channel = kraken::ws::SubscribeChannel::Ticker;
                    req.symbols = std::vector<std::string>{symbol};
                }
                else if (channel == "book")
                {
                    req.channel = kraken::ws::SubscribeChannel::Book;
                    req.symbols = std::vector<std::string>{symbol};
                }
                else if (channel == "level3")
                {
                    req.channel = kraken::ws::SubscribeChannel::Level3;
                    req.symbols = std::vector<std::string>{symbol};
                }
                else if (channel == "trade")
                {
                    req.channel = kraken::ws::SubscribeChannel::Trade;
                    req.symbols = std::vector<std::string>{symbol};
                }
                else if (channel == "ohlc")
                {
                    req.channel  = kraken::ws::SubscribeChannel::OHLC;
                    req.symbols  = std::vector<std::string>{symbol};
                    req.interval = 1;  // 1-minute candles
                }
                else if (channel == "instrument")
                {
                    req.channel = kraken::ws::SubscribeChannel::Instrument;
                    // no symbols for instrument
                }

                webSocket.send(req.to_json().dump());
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                spdlog::info("ws closed");
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                spdlog::error("ws error: {}", msg->errorInfo.reason);
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                auto j    = nlohmann::json::parse(msg->str);
                auto kind = kraken::ws::identify_message(j);

                switch (kind)
                {
                    case kraken::ws::MessageKind::SubscribeResponse:
                    {
                        auto resp = kraken::ws::SubscribeResponse::from_json(j);
                        spdlog::info("subscribe {}: {}", resp.method,
                                     resp.success ? "success" : "failure");
                        if (!resp.success && resp.error)
                            spdlog::error("  reason: {}", *resp.error);
                        break;
                    }
                    case kraken::ws::MessageKind::StatusMessage:
                    {
                        auto s = kraken::ws::StatusMessage::from_json(j);
                        spdlog::info("status: system={} version={}", s.system, s.version);
                        break;
                    }
                    case kraken::ws::MessageKind::Ticker:
                    {
                        auto m = kraken::ws::TickerMessage::from_json(j);
                        for (const auto& t : m.data)
                            spdlog::info("[ticker] {} bid={} ask={} last={} vol={} change={}%",
                                         t.symbol, t.bid, t.ask, t.last, t.volume, t.change_pct);
                        break;
                    }
                    case kraken::ws::MessageKind::Book:
                    {
                        auto m = kraken::ws::BookMessage::from_json(j);
                        for (const auto& b : m.data)
                        {
                            spdlog::info("[book/{}] {} bids={} asks={}", m.type, b.symbol,
                                         b.bids.size(), b.asks.size());
                            // Print top-3 bids and asks on snapshot
                            if (m.type == "snapshot")
                            {
                                for (std::size_t i = 0; i < b.bids.size() && i < 3; ++i)
                                    spdlog::info("  bid  price={} qty={}", b.bids[i].price, b.bids[i].qty);
                                for (std::size_t i = 0; i < b.asks.size() && i < 3; ++i)
                                    spdlog::info("  ask  price={} qty={}", b.asks[i].price, b.asks[i].qty);
                            }
                        }
                        break;
                    }
                    case kraken::ws::MessageKind::Level3:
                    {
                        // Level3Message is not yet fully typed; log raw JSON
                        spdlog::info("[level3] {}", j.dump());
                        break;
                    }
                    case kraken::ws::MessageKind::Trade:
                    {
                        auto m = kraken::ws::TradeMessage::from_json(j);
                        for (const auto& t : m.data)
                            spdlog::info("[trade] {} side={} price={} qty={} ts={}",
                                         t.symbol, t.side, t.price, t.qty, t.timestamp);
                        break;
                    }
                    case kraken::ws::MessageKind::OHLC:
                    {
                        auto m = kraken::ws::OHLCMessage::from_json(j);
                        for (const auto& o : m.data)
                            spdlog::info("[ohlc/{}] {} o={} h={} l={} c={} vol={} trades={}",
                                         m.type, o.symbol, o.open, o.high, o.low, o.close,
                                         o.volume, o.trades);
                        break;
                    }
                    case kraken::ws::MessageKind::Instrument:
                    {
                        auto m = kraken::ws::InstrumentMessage::from_json(j);
                        spdlog::info("[instrument/{}] {} instruments", m.type, m.data.size());
                        for (const auto& i : m.data)
                            spdlog::info("  {} base={} quote={} status={} qty_min={}",
                                         i.symbol, i.base, i.quote, i.status, i.qty_min);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    );

    webSocket.start();
    std::this_thread::sleep_for(std::chrono::seconds(30));
    webSocket.stop();

    return 0;
}
