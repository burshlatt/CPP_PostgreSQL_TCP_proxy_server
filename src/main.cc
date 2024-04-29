#include <iostream>

#include "server.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2) {
        Server server(std::stoi(argv[1]));
        server.Start();
    } else {
        std::cerr << "./server <port>\n";
    }

    return 0;
}