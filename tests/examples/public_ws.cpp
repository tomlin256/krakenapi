// ws.cpp
#include <ixwebsocket/IXWebSocket.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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

            nlohmann::json sub = {
               {"method", "subscribe"},
               {"params", {
                  {"channel", "ticker"},
                  {"symbol", nlohmann::json::array({symbol})}
               }}
            };
            webSocket.send(sub.dump());
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
            auto json = nlohmann::json::parse(msg->str);
            if (json.contains("method"))
            {
               spdlog::info("{}: {}", 
                  json["method"].get<std::string>(), 
                  json["success"].get<bool>() ? "success" : "failure");
            }
            else if (json.contains("channel")) 
            {
               std::string channel = json["channel"];
               if (channel == "ticker")
               {
                  auto data = json["data"].get<std::vector<nlohmann::json>>();
                  for (auto datum : data) {
                     std::string symbol = datum["symbol"];
                     float bid = datum["bid"];
                     float ask = datum["ask"];
                     spdlog::info("Ticker update for {}: bid={}, ask={}", symbol, bid, ask);
                  }
               }
               else
               {
                  spdlog::info("channel {}", channel);
               }
            }
         }
      }
   );

   webSocket.start();
   std::this_thread::sleep_for(std::chrono::seconds(10));
   webSocket.stop();

   return 0;
}