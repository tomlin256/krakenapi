// ws.cpp
#include "kraken_rest_client.hpp"
#include "kraken_ws_api.hpp"

#include <ixwebsocket/IXWebSocket.h>
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

   ix::WebSocket webSocket;
   webSocket.setUrl("wss://ws-auth.kraken.com/v2");

   webSocket.setOnMessageCallback(
      [&](const ix::WebSocketMessagePtr& msg)
      {
         if (msg->type == ix::WebSocketMessageType::Open)
         {
            spdlog::info("ws opened");

            kraken::ws::SubscribeRequest req;
            req.req_id = 10;
            req.channel = kraken::ws::SubscribeChannel::Balances;
            req.token   = token;
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
                  auto r = kraken::ws::SubscribeResponse::from_json(j);
                  spdlog::info("subscribe {}({}): {}", 
                     r.method, 
                     r.req_id.value_or(-1), 
                     r.success ? "success" : "failure");
                  break;
               }
               case kraken::ws::MessageKind::Balances:
               {
                  auto bals = kraken::ws::BalancesMessage::from_json(j);
                  for (const auto& b : bals.data)
                     spdlog::info("Balance {}: {:.8f} (hold: {:.8f})", b.asset, b.balance, b.hold_trade);
                  break;
               }
               default:
                  spdlog::debug("ws message: {}", msg->str);
                  break;
            }
         }
      }
   );

   webSocket.start();
   std::this_thread::sleep_for(std::chrono::seconds(10));
   webSocket.stop();

   curl_global_cleanup();
   return 0;
}