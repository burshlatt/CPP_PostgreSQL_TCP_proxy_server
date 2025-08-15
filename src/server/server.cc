#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "server.h"

static volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_flag = 1;
    }
}

Server::Server(int listen_port, const std::string& db_host, int db_port, const std::string& log_file) :
    _listen_port(CheckPort(listen_port)),
    _db_host(CheckHost(db_host)),
    _db_port(CheckPort(db_port)),
    _logger(_db_host, _db_port, log_file)
{}

int Server::CheckPort(int port) {
    if (port > 0 && port <= 65535) {
        return port;
    }

    throw std::invalid_argument("Invalid port: " + std::to_string(port));
}

std::string Server::CheckHost(const std::string& host) {
    struct in_addr addr;

    if (inet_pton(AF_INET, host.c_str(), &addr) == 1) {
        return host;
    }

    throw std::invalid_argument("Invalid host: " + host);
}

void Server::SetupEpoll() {
    _epoll_fd = UniqueFD(epoll_create1(0));
    
    if (!_epoll_fd.Valid()) {
        throw std::runtime_error("SetupEpoll(): " + std::string(strerror(errno)));
    }
}

void Server::UpdateEpollEvents(int fd, uint32_t events) {
    epoll_event event;
    event.data.fd = fd;
    event.events = events;

    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

void Server::SetupServerSocket() {
    _proxy_fd = UniqueFD(socket(AF_INET, SOCK_STREAM, 0));

    if (!_proxy_fd.Valid()) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    int flags{fcntl(_proxy_fd, F_GETFL, 0)};
    fcntl(_proxy_fd, F_SETFL, flags | O_NONBLOCK);

    int opt{1};

    if (setsockopt(_proxy_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_listen_port);

    auto s_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (bind(_proxy_fd, s_addr, sizeof(server_addr)) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    if (listen(_proxy_fd, SOMAXCONN) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _proxy_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _proxy_fd, &event) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }
}

UniqueFD Server::SetupPGSQLSocket() {
    UniqueFD pgsql_fd(socket(AF_INET, SOCK_STREAM, 0));

    if (!pgsql_fd.Valid()) {
        throw std::runtime_error("SetupPGSQLSocket(): " + std::string(strerror(errno)));
    }

    struct sockaddr_in pgsql_addr = {};
    pgsql_addr.sin_family = AF_INET;
    pgsql_addr.sin_port = htons(_db_port);

    if (inet_pton(AF_INET, _db_host.c_str(), &pgsql_addr.sin_addr) <= 0) {
        throw std::runtime_error("SetupPGSQLSocket(): " + std::string(strerror(errno)));
    }

    auto p_addr{reinterpret_cast<struct sockaddr*>(&pgsql_addr)};

    if (connect(pgsql_fd, p_addr, sizeof(pgsql_addr))) {
        throw std::runtime_error("SetupPGSQLSocket(): " + std::string(strerror(errno)));
    }

    int flags{fcntl(pgsql_fd, F_GETFL, 0)};
    fcntl(pgsql_fd, F_SETFL, flags | O_NONBLOCK);

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = pgsql_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, pgsql_fd, &event) == -1) {
        throw std::runtime_error("SetupPGSQLSocket(): " + std::string(strerror(errno)));
    }

    return pgsql_fd;
}

void Server::AcceptNewConnections() {
    while (true) {
        struct sockaddr_in client_addr = {};
        auto c_addr{reinterpret_cast<sockaddr*>(&client_addr)};
        socklen_t c_addr_len{sizeof(client_addr)};

        UniqueFD client_fd(accept(_proxy_fd, c_addr, &c_addr_len));

        if (!client_fd.Valid()) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "accept(): " << strerror(errno) << '\n';

                break;
            }
        }

        int flags{fcntl(client_fd, F_GETFL, 0)};
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd;

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            std::cerr << "epoll_ctl(): " << strerror(errno) << '\n';

            continue;
        }

        try {
            UniqueFD pgsql_fd{SetupPGSQLSocket()};

            auto session{std::make_shared<Session>(std::move(pgsql_fd), std::move(client_fd),
            [this](int fd, uint32_t events) {
                UpdateEpollEvents(fd, events);
            })};

            _fd_session_ht[session->GetPGSQLFD()] = session;
            _fd_session_ht[session->GetClientFD()] = session;

            Endpoint& client_ep{_fd_endpoint_ht[session->GetClientFD()]};
            client_ep.ip = inet_ntoa(client_addr.sin_addr);
            client_ep.port = ntohs(client_addr.sin_port);

            _logger.PrintInTerminal(client_ep, ConnectionStatus::K_OPEN);
        } catch (const std::exception& e) {
            std::cerr << "ConnectToPGSQL() connection failed: " << e.what() << '\n';
        }
    }
}

void Server::CloseSession(std::shared_ptr<Session> session) {
    int pgsql_fd{session->GetPGSQLFD()};
    int client_fd{session->GetClientFD()};

    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, pgsql_fd, nullptr);
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);

    _fd_session_ht.erase(pgsql_fd);
    _fd_session_ht.erase(client_fd);

    _logger.PrintInTerminal(_fd_endpoint_ht[client_fd], ConnectionStatus::K_CLOSED);

    _fd_endpoint_ht.erase(client_fd);
}

void Server::HandleEvent(epoll_event& event) {
    int fd{event.data.fd};

    auto it{_fd_session_ht.find(fd)};

    if (it == _fd_session_ht.end()) {
        return;
    }

    auto& session{it->second};

    int peer_fd{session->GetPeerFD(fd)};

    if (event.events & EPOLLOUT) {
        if (!session->TrySend(fd)) {
            CloseSession(session);
        }

        return;
    }

    if (!session->RecvAll(fd)) {
        CloseSession(session);

        return;
    }

    if (session->IsClientFD(fd)) {
        auto message{session->GetDataToPGSQL()};

        _logger.SaveLogs(_fd_endpoint_ht[fd], message);
    }

    if (!session->TrySend(peer_fd)) {
        CloseSession(session);
    }
}

void Server::EventLoop() {
    std::cout << "Waiting...\n";

    constexpr size_t MAX_EVENTS{1024};
    std::vector<epoll_event> events(MAX_EVENTS);

    while (!stop_flag) {
        int num_events{epoll_wait(_epoll_fd, events.data(), MAX_EVENTS, -1)};

        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("epoll_wait(): " + std::string(strerror(errno)));
        }

        for (int i{}; i < num_events; ++i) {
            int fd{events[i].data.fd};

            if (fd == _proxy_fd) {
                AcceptNewConnections();
            } else {
                HandleEvent(events[i]);
            }
        }
    }
}

void Server::Run() {
    std::signal(SIGINT, signal_handler);

    SetupEpoll();
    SetupServerSocket();
    EventLoop();
}
