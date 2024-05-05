#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <string>
#include <string_view>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unordered_map>

class Server {
public:
    explicit Server(unsigned port);
    ~Server();

public:
    void Start();

private:
    void EventLoop();
    void SetupEpoll();
    void SetupSocket();
    void DisableSSL(epoll_event& event);
    void SaveLogs(std::string_view request);
    void HandleClientEvent(epoll_event& event);

    int ConnectToPGSQL();

    bool IsSSLRequest(char* buffer);
    bool AcceptNewConnection(epoll_event& event);
    
private:
    unsigned port_;

    int s_socket_{};
    int epoll_fd_{};
    
    struct sockaddr_in s_addr_;

    std::unordered_map<int, int> pgsql_socks_;

    static constexpr unsigned max_events_{512};
    static constexpr unsigned max_buffer_size_{4096};
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
