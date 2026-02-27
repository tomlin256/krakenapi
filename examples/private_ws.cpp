// ws.cpp
#include "../kapi.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <fmt/format.h>

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
   std::cout << "Token: " << token << std::endl;

   // using v2 api,
   ix::WebSocket webSocket;
   webSocket.setUrl("wss://ws-auth.kraken.com/v2");

   webSocket.setOnMessageCallback(
      [&](const ix::WebSocketMessagePtr& msg)
      {
         if (msg->type == ix::WebSocketMessageType::Open)
         {
            std::cout << "ws opened" << std::endl;

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
            std::cout << "ws closed" << std::endl;
         }
         else if (msg->type == ix::WebSocketMessageType::Error)
         {
            std::cout << "ws error: " << msg->errorInfo.reason << std::endl;
         }
         else if (msg->type == ix::WebSocketMessageType::Message)
         {
            std::cout << "ws message: " << msg->str << std::endl;
         }
      }
   );

   webSocket.start();
   std::this_thread::sleep_for(std::chrono::seconds(10));
   webSocket.stop();

   return 0;
}