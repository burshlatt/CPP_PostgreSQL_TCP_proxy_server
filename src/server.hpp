#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <string>
#include <fstream>
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
    void SetNonBlocking(int sockfd) const;
    void SaveLogs(std::string_view request);
    void DisableSSL(epoll_event& event) const;
    void HandleClientEvent(epoll_event& event);

    int ConnectToPGSQL() const;

    bool IsSSLRequest(char* buffer) const;
    bool IsSQLRequest(std::string_view request) const;
    bool AcceptNewConnection(epoll_event& event) const;

    std::string GetSQLRequest(std::string_view request) const;
        
private:
    unsigned port_;

    int s_socket_{};
    int epoll_fd_{};
    
    struct sockaddr_in s_addr_;

    std::unordered_map<int, int> pgsql_sockets_;

    std::ofstream log_file_;

    static constexpr unsigned max_events_{1024};
    static constexpr unsigned max_buffer_size_{983040};
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
