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
            spdlog::info("ws message: {}", msg->str);
         }
      }
   );

   webSocket.start();
   std::this_thread::sleep_for(std::chrono::seconds(10));
   webSocket.stop();

   return 0;
}