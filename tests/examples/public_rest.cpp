#include <iostream>
#include <stdexcept>

#include "kapi.hpp"
#include "kraken_rest_client.hpp"

using namespace std;
using namespace kraken::rest;

int main()
{
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        KrakenRestClient client;

        GetRecentTradesRequest req;
        req.pair = "XXBTZEUR";

        auto resp = client.execute(req);
        if (resp.ok && resp.result) {
            cout << "pair: " << resp.result->pair << "\n";
            cout << "last: " << resp.result->last << "\n";
            cout << "trades (" << resp.result->trades.size() << "):\n";
            for (const auto& t : resp.result->trades)
                cout << "  price=" << t.price << " volume=" << t.volume
                     << " side=" << (t.side == kraken::Side::Buy ? "buy" : "sell") << "\n";
        } else {
            for (const auto& e : resp.errors)
                cerr << "Error: " << e << "\n";
        }
    }
    catch(exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    catch(...) {
        cerr << "Unknown exception." << endl;
    }

    curl_global_cleanup();
    return 0;
}
