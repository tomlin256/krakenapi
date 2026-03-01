#include <iostream>
#include <stdexcept>

#include "kapi.hpp"

using namespace std;
using namespace Kraken;

int main(int argc, char* argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    auto keys = load_keys("default");

    try {
        KAPI kapi(keys.apiKey, keys.privateKey);
        KAPI::Input in;
        cout << kapi.private_method("GetWebSocketsToken", in) << endl;
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
