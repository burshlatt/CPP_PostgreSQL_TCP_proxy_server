#include <iostream>

#include "server/server.h"

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <listen port> <database host> <database port> <log file>\n";

        return 0;
    }

    try {
        Server server(std::stoi(argv[1]), argv[2], std::stoi(argv[3]), argv[4]);
        server.Run();
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << '\n';
    }

    return 0;
}
