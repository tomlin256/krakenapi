// ws.cpp
#include "kraken_rest_client.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>

using namespace kraken::rest;

int main(int argc, char* argv[])
{
   curl_global_init(CURL_GLOBAL_ALL);

   // we need a token to subscribe to private channels, so we need to call a private method first
   KrakenRestClient client;
   auto creds = Credentials::from_file("default");
   auto resp = client.execute(GetWebSocketsTokenRequest{}, creds);
   if (!resp.ok || !resp.result) {
      for (const auto& e : resp.errors)
         spdlog::error("Error: {}", e);
      curl_global_cleanup();
      return 1;
   }
   std::string token = resp.result->token;
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

   curl_global_cleanup();
   return 0;
}