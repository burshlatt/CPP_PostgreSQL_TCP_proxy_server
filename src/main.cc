#include <iostream>

#include "server/server.h"

int main(int argc, char* argv[]) {
    if (argc == 2) {
        Server server(argv[1]);
        server.Start();
    } else {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
    }

    return 0;
}
