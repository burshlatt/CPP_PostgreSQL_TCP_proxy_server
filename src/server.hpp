#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <string_view>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>

class Server {
public:
    explicit Server(char* port);
    ~Server();

public:
    void Start();

private:
    // === ИНИЦИАЛИЗАЦИЯ ===
    int SetupEpoll();
    int SetupProxySocket();
    int SetupPGSQLSocket();

    // === ОСНОВНОЙ ЦИКЛ ===
    void EventLoop();

    // === ОБРАБОТКА КЛИЕНТОВ ===
    bool IsClientFD(int fd);
    int GetClientFD(int pgsql_fd);
    void AcceptNewConnections();
    void CloseConnection(int fd);
    void HandleEvent(epoll_event& event);

    // === ЛОГИ И SSL ===
    void DisableSSL(epoll_event& event);
    void SaveLogs(std::string_view request);

    // === SQL / SSL ЛОГИКА ===
    bool IsSQLRequest(std::string_view request) const;
    bool IsSSLRequest(const std::vector<char>& client_data) const;
    std::string_view GetSQLRequest(std::string_view request) const;

    // === СЕТЕВОЙ ВВОД/ВЫВОД ===
    ssize_t RecvAll(int fd, std::vector<char>& buffer);
    bool SendAll(int fd, const std::vector<char>& buffer);
        
private:
    unsigned _port;

    int _proxy_fd{};
    int _epoll_fd{};

    std::ofstream _log_file;
    
    struct sockaddr_in _s_addr;

    std::unordered_set<int> _ssl_disabled_clients_set;
    std::unordered_map<int, int> _client_psql_sockets_ht;
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
