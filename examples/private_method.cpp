#include <iostream>
#include <stdexcept>

#include "../kapi.hpp"

using namespace std;
using namespace Kraken;

int main(int argc, char* argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <api_key> <api_private_key>" << endl;
        return 1;
    }

    auto api_key = string(argv[1]);
    auto api_private_key = string(argv[2]);
    cout << "API Key: " << api_key << endl;
    cout << "API Private Key: " << api_private_key << endl;

    try {
        KAPI kapi(api_key, api_private_key);
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
