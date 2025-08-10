#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_INFO_CONNECTION_INFO_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_INFO_CONNECTION_INFO_H

#include <string>
#include <cstdint>

struct Endpoint {
    int fd;
    std::string ip;
    uint16_t port;
};

enum class ConnectionStatus : bool {
    K_OPEN,
    K_CLOSED
};

struct Connection {
    Endpoint pgsql;
    Endpoint client;

    ConnectionStatus conn_status;
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_INFO_CONNECTION_INFO_H
