#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sys/socket.h>

#include "server.h"

Server::Server(char* port) :
    _port(std::stoi(port))
{
    _s_addr.sin_family = AF_INET;
    _s_addr.sin_addr.s_addr = INADDR_ANY;
    _s_addr.sin_port = htons(_port);

    _log_file.open("requests.log", std::ios::app);

    if (!_log_file.is_open()) {
        throw std::runtime_error("Server::Server() error opening file: " + std::to_string(errno));
    }
}

Server::~Server() {
    close(_proxy_fd);
    close(_epoll_fd);

    for (auto& [client_fd, pgsql_fd] : _client_psql_sockets_ht) {
        close(pgsql_fd);
        close(client_fd);
    }
}

int Server::SetupProxySocket() {
    int proxy_fd{socket(AF_INET, SOCK_STREAM, 0)};

    if (proxy_fd == -1) {
        throw std::runtime_error("SetupProxySocket() error creating socket: " + std::to_string(errno));
    }

    int flags{fcntl(proxy_fd, F_GETFL, 0)};
    fcntl(proxy_fd, F_SETFL, flags | O_NONBLOCK);

    int opt{1};

    if (setsockopt(proxy_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("SetupProxySocket() error setting socket options: " + std::to_string(errno));
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = proxy_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, proxy_fd, &event) == -1) {
        throw std::runtime_error("SetupProxySocket() error adding socket to epoll: " + std::to_string(errno));
    }

    return proxy_fd;
}

int Server::SetupEpoll() {
    int epoll_fd{epoll_create1(0)};
    
    if (epoll_fd == -1) {
        throw std::runtime_error("SetupEpoll() error creating epoll instance: " + std::to_string(errno));
    }

    return epoll_fd;
}

int Server::SetupPGSQLSocket() {
    int pgsql_fd{socket(AF_INET, SOCK_STREAM, 0)};

    if (pgsql_fd == -1) {
        throw std::runtime_error("SetupPGSQLSocket() error creating socket: " + std::to_string(errno));
    }

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5432);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        close(pgsql_fd);

        throw std::runtime_error("SetupPGSQLSocket() error converting IP address: " + std::to_string(errno));
    }

    auto casted_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (connect(pgsql_fd, casted_addr, sizeof(server_addr))) {
        close(pgsql_fd);

        throw std::runtime_error("SetupPGSQLSocket() error connecting to postgresql server: " + std::to_string(errno));
    }

    int flags{fcntl(pgsql_fd, F_GETFL, 0)};
    fcntl(pgsql_fd, F_SETFL, flags | O_NONBLOCK);

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = pgsql_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, pgsql_fd, &event) == -1) {
        close(pgsql_fd);

        throw std::runtime_error("SetupPGSQLSocket() error adding socket to epoll: " + std::to_string(errno));
    }

    return pgsql_fd;
}

void Server::AcceptNewConnections() {
    while (true) {
        struct sockaddr_in c_addr;
        socklen_t c_addr_len{sizeof(c_addr)};
        int client_fd{accept(_proxy_fd, reinterpret_cast<sockaddr*>(&c_addr), &c_addr_len)};

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "accept() error: " << strerror(errno) << "\n";

                break;
            }
        }

        int flags{fcntl(client_fd, F_GETFL, 0)};
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd;

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            close(client_fd);

            std::cerr << "epoll_ctl() error: " << strerror(errno) << "\n";

            continue;
        }

        try {
            _client_psql_sockets_ht[client_fd] = SetupPGSQLSocket();
        } catch (const std::exception& e) {
            close(client_fd);

            _client_psql_sockets_ht.erase(client_fd);

            std::cerr << "ConnectToPGSQL() connection failed: " << e.what() << "\n";
        }
    }
}

bool Server::IsSQLRequest(std::string_view request) const {
    return !request.empty() && request[0] == 'Q';
}

std::string_view Server::GetSQLRequest(std::string_view request) const {
    if (request.back() == '\0') {
        request.remove_suffix(1); // Убираем нулевой терминатор
    }

    return request.substr(5); // Убираем первые 5 байт (1 - Тип сообщения, 4 - размер сообщения)
}

void Server::SaveLogs(std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string sql_req(GetSQLRequest(request));

    _log_file.write(sql_req.data(), sql_req.size());
    _log_file << std::endl;
}

bool Server::IsClientFD(int fd) {
    return _client_psql_sockets_ht.find(fd) != _client_psql_sockets_ht.end();
}

int Server::GetClientFD(int pgsql_fd) {
    for (const auto& it : _client_psql_sockets_ht) {
        if (it.second == pgsql_fd) {
            return it.first;
        }
    }

    return -1;
}

void Server::CloseConnection(int fd) {
    int pgsql_fd{-1};
    int client_fd{-1};

    if (IsClientFD(fd)) {
        client_fd = fd;
        pgsql_fd = _client_psql_sockets_ht[client_fd];

        _client_psql_sockets_ht.erase(client_fd);
    } else {
        pgsql_fd = fd;
        client_fd = GetClientFD(pgsql_fd);

        if (client_fd == -1) {
            std::cerr << "CloseConnection() unknown client_fd: " << fd << "\n";

            return;
        }
    }

    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, pgsql_fd, nullptr);
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);

    close(pgsql_fd);
    close(client_fd);
}

void Server::AddEpollOut(int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;

    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

void Server::RemoveEpollOut(int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

ssize_t Server::RecvAll(int fd, std::vector<char>& buffer) {
    ssize_t bytes_read{0};

    while (true) {
        char temp[4096];
        ssize_t n{recv(fd, temp, sizeof(temp), 0)};

        if (n > 0) {
            buffer.insert(buffer.end(), temp, temp + n);

            bytes_read += n;
        } else if (n == 0) {
            
            CloseConnection(fd);
        
            return n;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "recv() error from client: " << strerror(errno) << "\n";

            CloseConnection(fd);

            return n;
        }
    }

    return bytes_read;
}

bool Server::SendBuffer(int fd, const std::vector<char>& data) {
    auto& buffer{_send_buffers_ht[fd]};
    buffer.insert(buffer.end(), data.begin(), data.end());

    return TrySend(fd);
}

bool Server::TrySend(int fd) {
    size_t bytes_sent{0};
    auto& buffer{_send_buffers_ht[fd]};

    while (bytes_sent < buffer.size()) {
        ssize_t n{send(fd, buffer.data() + bytes_sent, buffer.size() - bytes_sent, 0)};
        
        if (n > 0) {
            bytes_sent += n;
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                AddEpollOut(fd);

                buffer.erase(buffer.begin(), buffer.begin() + bytes_sent);

                return true;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "send() error to PostgreSQL: " << strerror(errno) << "\n";

            CloseConnection(fd);

            return false;
        }
    }

    RemoveEpollOut(fd);

    _send_buffers_ht.erase(fd);

    return true;
}

bool Server::IsSSLRequest(const std::vector<char>& client_data) const {
    if (client_data.size() != 8) {
        return false;
    }
    
    uint32_t received_msg{0};

    std::memcpy(&received_msg, client_data.data() + 4, sizeof(int));

    received_msg = ntohl(received_msg);

    const uint32_t ssl_request_code{80877103};

    if (received_msg == ssl_request_code) {
        return true;
    }

    return false;
}

void Server::DisableSSL(epoll_event& event) {
    int client_fd{event.data.fd};
    const char ssl_deny_msg{'N'};

    if (send(client_fd, &ssl_deny_msg, 1, 0) < 0) {
        std::cerr << "DisableSSL() error: " << strerror(errno) << "\n";
    } else {
        _ssl_disabled_clients_set.insert(client_fd);
    }
}

bool Server::SSLDisabled(int fd) const {
    return _ssl_disabled_clients_set.find(fd) != _ssl_disabled_clients_set.end();
}

void Server::HandleEvent(epoll_event& event) {
    int peer_fd{-1};
    int fd{event.data.fd};
    bool is_client{IsClientFD(fd)};

    if (is_client) {
        peer_fd = _client_psql_sockets_ht[fd];
    } else {
        peer_fd = GetClientFD(fd);
    }

    if (peer_fd == -1) {
        std::cerr << "HandleEvent() unknown fd in epoll: " << fd << "\n";

        return;
    }

    if (event.events & EPOLLOUT) {
        TrySend(fd);

        return;
    }

    std::vector<char> data;

    if (RecvAll(fd, data) <= 0) {
        return;
    }

    if (data.empty()) {
        return;
    }

    if (is_client) {
        if (!SSLDisabled(fd)) {
            if (IsSSLRequest(data)) {
                DisableSSL(event);

                return;
            }
        }

        SaveLogs(std::string_view(data.data(), data.size()));
    }

    SendBuffer(peer_fd, data);
}

void Server::EventLoop() {
    std::cout << "Waiting...\n";

    const unsigned max_events{1024};
    std::vector<epoll_event> events(max_events);

    while (true) {
        int num_events{epoll_wait(_epoll_fd, events.data(), max_events, -1)};

        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("epoll_wait() failed: " + std::string(strerror(errno)));
        }

        for (int i{}; i < num_events; ++i) {
            int fd{events[i].data.fd};

            if (fd == _proxy_fd) {
                AcceptNewConnections();

                std::cout << "Accepted the new connection.\n";
            } else {
                HandleEvent(events[i]);
            }
        }
    }
}

void Server::Start() {
    _epoll_fd = SetupEpoll();
    _proxy_fd = SetupProxySocket();

    auto s_addr{reinterpret_cast<struct sockaddr*>(&_s_addr)};

    if (bind(_proxy_fd, s_addr, sizeof(_s_addr)) == -1) {
        throw std::runtime_error("Socket binding error!");
    }

    if (listen(_proxy_fd, SOMAXCONN) == -1) {
        throw std::runtime_error("Socket listening Error!");
    }

    EventLoop();
}
