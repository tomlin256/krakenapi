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

int main(int argc, char* argv[])
{
   ix::WebSocket webSocket;

   std::string symbol;
   if (argc > 1)
   {
      symbol = argv[1];
   }
   else
   {
      throw std::runtime_error("Usage: public_ws <symbol>");
   }

   webSocket.setUrl("wss://ws.kraken.com/v2");

   webSocket.setOnMessageCallback(
      [&](const ix::WebSocketMessagePtr& msg)
      {
         if (msg->type == ix::WebSocketMessageType::Open)
         {
            spdlog::info("ws opened");

            kraken::ws::SubscribeRequest req;
            req.channel = kraken::ws::SubscribeChannel::Ticker;
            req.symbols = std::vector<std::string>{symbol};
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
            auto j = nlohmann::json::parse(msg->str);
            switch (kraken::ws::identify_message(j))
            {
               case kraken::ws::MessageKind::SubscribeResponse:
               {
                  auto resp = kraken::ws::SubscribeResponse::from_json(j);
                  spdlog::info("subscribe {}: {}", resp.method, resp.success ? "success" : "failure");
                  break;
               }
               case kraken::ws::MessageKind::Ticker:
               {
                  auto ticker = kraken::ws::TickerMessage::from_json(j);
                  for (const auto& t : ticker.data)
                     spdlog::info("Ticker update for {}: bid={}, ask={}", t.symbol, t.bid, t.ask);
                  break;
               }
               default:
                  break;
            }
         }
      }
   );

   webSocket.start();
   std::this_thread::sleep_for(std::chrono::seconds(10));
   webSocket.stop();

   return 0;
}