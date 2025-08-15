#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_CONNECTION_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_CONNECTION_H

#include <string>
#include <cstdint>

/**
 * @brief Статус соединения клиента с сервером.
 */
enum class ConnectionStatus : bool {
    K_OPEN, ///< Соединение открыто
    K_CLOSED ///< Соединение закрыто
};

/**
 * @brief Информация о сетевом конечном пункте (IP и порт).
 */
struct Endpoint {
    std::string ip; ///< IP-адрес
    uint16_t port; ///< Порт
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_CONNECTION_CONNECTION_H
