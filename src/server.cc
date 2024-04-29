#include <iostream>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>

#include "server.hpp"

Server::Server(unsigned port) :
    port_(port),
    connection_("dbname=sbtest user=sbtest password=12345 hostaddr=127.0.0.1 port=5432")
{
    if (!connection_.is_open()) {
        throw std::runtime_error("Connection error!");
    }

    s_addr_.sin_family = AF_INET;
    s_addr_.sin_addr.s_addr = INADDR_ANY;
    s_addr_.sin_port = htons(port_);
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

void Server::HandleClientEvent(epoll_event& event) {
    char buffer[max_buffer_size_];
    ssize_t bytes_read{recv(event.data.fd, buffer, max_buffer_size_, 0)};

    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
        close(event.data.fd);
    } else {
        std::string request(buffer, bytes_read);

        SaveLogs(request);

        // request = "SELECT COUNT(*) FROM sbtest1;";

        pqxx::work txn(connection_);
        pqxx::result result{txn.exec(request)};

        txn.commit();

        // for (auto const &row: result) {
        //     for (auto const &field: row)
        //         std::cout << field.c_str() << '\t';

        //     std::cout << '\n';
        // }
    }
}

void Server::DisableSSL(epoll_event& event) {
    char ssl_off{'N'};
    char buffer[max_buffer_size_];
    recv(event.data.fd, buffer, max_buffer_size_, 0);
    send(event.data.fd, &ssl_off, 1, 0);
}

void Server::EventLoop() {
    std::vector<struct epoll_event> events(max_events_);

    while (true) {
        int num_events{epoll_wait(epoll_fd_, events.data(), max_events_, -1)};

        for (int i{}; i < num_events; ++i) {
            if (events[i].data.fd == s_socket_) {
                AcceptNewConnection(events[i]);
            } else {
                /*
                    Расчитано на то, что каждый запрос сперва является SSLRequest,
                    если первым запросом будет не SSLRequest, то отработает некорректно
                */
                DisableSSL(events[i]);
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
