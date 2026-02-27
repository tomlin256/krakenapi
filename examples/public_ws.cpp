// ws.cpp
#include <ixwebsocket/IXWebSocket.h>

#include <fmt/format.h>

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
      throw std::runtime_error("Usage: ws <symbol>");
   }

   webSocket.setUrl("wss://ws.kraken.com/v2");

   webSocket.setOnMessageCallback(
      [&](const ix::WebSocketMessagePtr& msg)
      {
         if (msg->type == ix::WebSocketMessageType::Open)
         {
            std::cout << "ws opened" << std::endl;

            std::string sub = R"({{
                "method": "subscribe",
                "params": {{
                    "channel": "ticker",
                    "symbol": ["{}"]
                }}
            }})";
            sub = fmt::format(sub, symbol);

            webSocket.send(sub);
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