#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <string>
#include <arpa/inet.h>
#include <sys/epoll.h>

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
    void SaveLogs(const std::string& request);
    void HandleClientEvent(epoll_event& event);
    void AcceptNewConnection(epoll_event& event);

    int ConnectToPGSQL();
    
private:
    unsigned port_;

    int s_socket_{};
    int epoll_fd_{};

    int pgsql_socket{};
    
    struct sockaddr_in s_addr_;

    static constexpr unsigned max_events_{512};
    static constexpr unsigned max_buffer_size_{4096};
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
