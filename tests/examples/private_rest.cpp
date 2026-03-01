#include <iostream>
#include <stdexcept>

#include "kraken_rest_client.hpp"

using namespace std;
using namespace kraken::rest;

int main(int argc, char* argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        KrakenRestClient client;
        auto creds = Credentials::from_file("default");

        auto resp = client.execute(GetWebSocketsTokenRequest{}, creds);
        if (resp.ok && resp.result) {
            cout << "token:   " << resp.result->token   << "\n";
            cout << "expires: " << resp.result->expires << "\n";
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
