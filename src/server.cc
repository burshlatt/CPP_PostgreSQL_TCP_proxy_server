#include <iostream>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>

#include "server.hpp"

Server::Server(unsigned port) :
    port_(port)
{
    s_addr_.sin_family = AF_INET;
    s_addr_.sin_addr.s_addr = INADDR_ANY;
    s_addr_.sin_port = htons(port_);

    pgsql_socket = ConnectToPGSQL();
}

Server::~Server() {
    close(s_socket_);
    close(epoll_fd_);
}

void Server::SetupSocket() {
    s_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (s_socket_ == -1) {
        throw std::runtime_error("Error: socket()");
    }

    int opt{1};

    if (setsockopt(s_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("Error: setsockopt()");
    }
}

void Server::SetupEpoll() {
    epoll_fd_ = epoll_create1(0);
    
    if (epoll_fd_ == -1) {
        throw std::runtime_error("Error: epoll_create1()");
    }

    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = s_socket_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, s_socket_, &event) == -1) {
        throw std::runtime_error("Error: epoll_ctl()");
    }
}

void Server::AcceptNewConnection(epoll_event& event) {
    struct sockaddr_in c_addr;
    socklen_t c_addr_len{sizeof(c_addr)};
    auto casted_c_addr{reinterpret_cast<struct sockaddr*>(&c_addr)};
    int c_socket{accept(s_socket_, casted_c_addr, &c_addr_len)};

    if (c_socket == -1) {
        std::cerr << "Error: Failed to accept connection\n";
    } else {
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = c_socket;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, c_socket, &event) == -1) {
            std::cerr << "Error: Failed to add client socket to epoll\n";
            close(c_socket);
        }
    }
}

void Server::SaveLogs(const std::string& request) {
    std::ofstream log_file("requests.log", std::ios::app);

    log_file << request << '\n';
}

void Server::DisableSSL(epoll_event& event) {
    char buffer[max_buffer_size_];
    ssize_t bytes_read{recv(event.data.fd, buffer, max_buffer_size_, 0)};

    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
        close(event.data.fd);
    } else {
        if (send(event.data.fd, "N", 1, 0) < 0) {
            throw std::runtime_error("Error: send()");
        }
    }
}

int Server::ConnectToPGSQL() {
    int pgsql_socket{socket(AF_INET, SOCK_STREAM, 0)};

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5432);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        close(pgsql_socket);
        throw std::runtime_error("Error connecting to postgresql srver!");
    }

    auto casted_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (connect(pgsql_socket, casted_addr, sizeof(server_addr))) {
        close(pgsql_socket);
        throw std::runtime_error("Error connecting to postgresql srver!");
    }

    return pgsql_socket;
}

bool IsSSLRequest(char* buffer) {
    static constexpr int ssl_request_code{0x04d2162f};

    char tmp_buffer[8]{
        buffer[0], buffer[1], buffer[2], buffer[3],
        buffer[4], buffer[5], buffer[6], buffer[7],
    };

    uint32_t msg_len{ntohl(*reinterpret_cast<int*>(tmp_buffer))};
    uint32_t ssl_code{ntohl(*reinterpret_cast<int*>(tmp_buffer + 4))};

    if (msg_len == 8 && ssl_code == ssl_request_code) {
        return true;
    }

    return false;
}

void Server::HandleClientEvent(epoll_event& event) {
    // DisableSSL(event);

    char buffer[max_buffer_size_];
    ssize_t bytes_read{recv(event.data.fd, buffer, max_buffer_size_, 0)};

    // if (IsSSLRequest(buffer)) {
        // close(pgsql_socket);
        // pgsql_socket = ConnectToPGSQL();
    // }

    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
        close(event.data.fd);
    } else {
        SaveLogs(std::string(buffer, bytes_read));

        if (send(pgsql_socket, buffer, bytes_read, 0) < 0) {
            throw std::runtime_error("Error: send()");
        }

        bytes_read = recv(pgsql_socket, buffer, max_buffer_size_, 0);

        if (bytes_read <= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
            close(event.data.fd);
        } else {
            if (send(event.data.fd, buffer, bytes_read, 0) < 0) {
                throw std::runtime_error("Error: send()");
            }
        }
    }
}

void Server::EventLoop() {
    std::vector<struct epoll_event> events(max_events_);

    while (true) {
        int num_events{epoll_wait(epoll_fd_, events.data(), max_events_, -1)};

        for (int i{}; i < num_events; ++i) {
            if (events[i].data.fd == s_socket_) {
                AcceptNewConnection(events[i]);
            } else {
                HandleClientEvent(events[i]);
            }
        }
    }
}

void Server::Start() {
    SetupSocket();

    auto casted_s_addr{reinterpret_cast<struct sockaddr*>(&s_addr_)};

    if (bind(s_socket_, casted_s_addr, sizeof(s_addr_)) == -1) {
        throw std::runtime_error("Error: bind()");
    }

    if (listen(s_socket_, SOMAXCONN) == -1) {
        throw std::runtime_error("Error: listen()");
    }

    SetupEpoll();
    EventLoop();
}
