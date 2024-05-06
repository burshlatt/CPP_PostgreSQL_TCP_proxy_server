#include <iostream>

#include "server.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2) {
        Server server(std::stoi(argv[1]));
        server.Start();
    } else {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
    }

    return 0;
}