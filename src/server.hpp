#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <mutex>
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
    void DisableSSL(epoll_event& event);
    void SaveLogs(std::string_view request);
    void HandleClientEvent(epoll_event& event);

    int ConnectToPGSQL();

    bool IsSSLRequest(char* buffer);
    bool IsSQLRequest(std::string_view request);
    bool AcceptNewConnection(epoll_event& event);

    std::string GetSQLRequest(std::string_view request);

    ssize_t SendAll(int fd, const char* buf, size_t len);
    
private:
    unsigned port_;

    int s_socket_{};
    int epoll_fd_{};
    
    struct sockaddr_in s_addr_;

    std::unordered_map<int, int> pgsql_sockets_;

    std::mutex mutex_;

    std::ofstream log_file_;

    static constexpr unsigned max_events_{512};
    static constexpr unsigned max_buffer_size_{32768};
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
