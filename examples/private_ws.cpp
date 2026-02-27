// ws.cpp
#include "../kapi.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include <iostream>

using namespace Kraken;

int main(int argc, char* argv[])
{

   // we need a token to subscribe to private channels, so we need to call a private method first
   auto keys = load_keys("default");
   KAPI kapi(keys.apiKey, keys.privateKey);
   auto tokenResponse = kapi.private_method("GetWebSocketsToken");
   auto json = nlohmann::json::parse(tokenResponse);
   std::string token = json["result"]["token"];
   spdlog::info("Token: {}", token);

   // using v2 api,
   ix::WebSocket webSocket;
   webSocket.setUrl("wss://ws-auth.kraken.com/v2");

   webSocket.setOnMessageCallback(
      [&](const ix::WebSocketMessagePtr& msg)
      {
         if (msg->type == ix::WebSocketMessageType::Open)
         {
            spdlog::info("ws opened");

            nlohmann::json sub = {
               {"method", "subscribe"},
               {"params", {
                  {"channel", "balances"},
                  {"token", token}
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