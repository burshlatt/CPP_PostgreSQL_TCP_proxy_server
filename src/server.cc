#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sys/socket.h>

#include "server.hpp"

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

/*
    Функция установки и настройки сокета прокси сервера.
*/
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

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = proxy_fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, proxy_fd, &event) == -1) {
        throw std::runtime_error("SetupProxySocket() error adding socket to epoll: " + std::to_string(errno));
    }

    return proxy_fd;
}

/*
    Функция получения дескриптора epoll и добавления дескриптора прокси сервера в список просматриваемых.
*/
int Server::SetupEpoll() {
    int epoll_fd{epoll_create1(0)};
    
    if (epoll_fd == -1) {
        throw std::runtime_error("SetupEpoll() error creating epoll instance: " + std::to_string(errno));
    }

    return epoll_fd;
}

/*
    Функция открытия и настройки нового сокета psql сервера для нового клиента.
    В случае успешного открытия добавляет дескритор в список просматриваемых дескрипторов в epoll.
*/
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

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, pgsql_fd, &event) == -1) { // ??? ТУТ ЗАКРЫВАТЬ СОКЕТ pgsql_fd ???
        throw std::runtime_error("SetupPGSQLSocket() error adding socket to epoll: " + std::to_string(errno));
    }

    return pgsql_fd;
}

/*
    Функция принимающая новое соединение,
    в случае успешного принятия добавляет дескритор клиента в список просматриваемых дескрипторов в epoll.
    Устанавливает флаг EPOLLET для epoll, что означает что epoll будет работать в режиме edge-triggered,
    то есть будет возвращать только те дескрипторы, в которых были совершены события
*/
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
            std::cerr << "epoll_ctl() error: " << strerror(errno) << "\n";

            close(client_fd);

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

/*
    Функция проверки на то, является ли запрос SQL запросом, чтобы в дальнейшем логировать его в файл.
*/
bool Server::IsSQLRequest(std::string_view request) const {
    return !request.empty() && request[0] == 'Q';
}

/*
    Функция пполучения подстроки с SQL запросом из строки, которая была передана в формате из сетевого протокола PostgreSQL
*/
std::string_view Server::GetSQLRequest(std::string_view request) const {
    if (request.back() == '\0') {
        request.remove_suffix(1); // Убираем нулевой терминатор
    }

    return request.substr(5); // Убираем первые 5 байт (1 - Тип сообщения, 4 - размер сообщения)
}

/*
    Функция логирования запросов в файл.
*/
void Server::SaveLogs(std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string sql_req(GetSQLRequest(request));

    _log_file.write(sql_req.data(), sql_req.size());
    _log_file << std::endl;

    // _log_file.write(request.data(), request.size());
    // _log_file << std::endl;
}

/*
    Функция проверки является ли этот дескриптор клиентским.
*/
bool Server::IsClientFD(int fd) {
    return _client_psql_sockets_ht.find(fd) != _client_psql_sockets_ht.end();
}

/*
    Функция получение связанного клиентского дескриптора с pgsql дескриптором.
*/
int Server::GetClientFD(int pgsql_fd) {
    for (const auto& it : _client_psql_sockets_ht) {
        if (it.second == pgsql_fd) {
            return it.first;
        }
    }

    return -1;
}

/*
    Функция закрытия соединения между клиентом и pgsql сервером.
*/
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

/*
    Функция чтения данных.
*/
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

/*
    Функция отправки данных.
*/
bool Server::SendAll(int fd, const std::vector<char>& buffer) {
    size_t bytes_sent{0};

    while (bytes_sent < buffer.size()) {
        ssize_t n{send(fd, buffer.data() + bytes_sent, buffer.size() - bytes_sent, 0)};
        
        if (n > 0) {
            bytes_sent += n;
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "send() error to PostgreSQL: " << strerror(errno) << "\n";

            CloseConnection(fd);

            return false;
        }
    }

    return true;
}

/*
    Функция проверки является ли запрос SSLRequest'ом, чтобы в дальнейшем отклонить SSL соединение.
*/
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

/*
    Функция передачи отказа от SSL соединения клиенту.
*/
void Server::DisableSSL(epoll_event& event) {
    int client_fd{event.data.fd};
    const char ssl_deny_msg{'N'};

    if (send(client_fd, &ssl_deny_msg, 1, 0) < 0) {
        std::cerr << "DisableSSL() error: " << strerror(errno) << "\n";
    } else {
        _ssl_disabled_clients_set.insert(client_fd);
    }
}

/*
    Функция обработчик событий,
    реализует передачу данных от клиента на сервер PostgreSQL и обратно,
    попутно логируя данные в файл.
*/
void Server::HandleEvent(epoll_event& event) {
    int peer_fd{-1};
    int fd{event.data.fd};
    bool is_client{_client_psql_sockets_ht.find(fd) != _client_psql_sockets_ht.end()};

    if (is_client) {
        peer_fd = _client_psql_sockets_ht[fd];
    } else {
        for (const auto& [client_fd, pgsql_fd] : _client_psql_sockets_ht) {
            if (pgsql_fd == fd) {
                peer_fd = client_fd;
                
                break;
            }
        }
    }

    if (peer_fd == -1) {
        std::cerr << "HandleEvent() unknown fd in epoll: " << fd << "\n";

        return;
    }

    std::vector<char> data;

    if (RecvAll(fd, data) <= 0) {
        return;
    }

    if (!data.empty()) {
        if (is_client) {
            bool ssl_disabled{_ssl_disabled_clients_set.find(fd) != _ssl_disabled_clients_set.end()};

            if (!ssl_disabled) {
                if (IsSSLRequest(data)) {
                    DisableSSL(event);

                    return;
                }
            }

            SaveLogs(std::string_view(data.data(), data.size()));
        }

        if (!SendAll(peer_fd, data)) {
            return;
        }
    }
}

/*
    Функция ожидания новых событий или подключений.
*/
void Server::EventLoop() {
    std::cout << "Waiting...\n";

    const unsigned max_events{1024};
    std::vector<struct epoll_event> events(max_events);

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

        events.clear();
        events.reserve(max_events);
    }
}

/*
    Основная функция запуска сервера.
*/
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
