#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sys/socket.h>

#include "server.hpp"

Server::Server(char* port) :
    port_(std::stoi(port))
{
    s_addr_.sin_family = AF_INET;
    s_addr_.sin_addr.s_addr = INADDR_ANY;
    s_addr_.sin_port = htons(port_);

    log_file_.open("requests.log", std::ios::app);

    if (!log_file_.is_open()) {
        throw std::runtime_error("Error opening file: " + std::to_string(errno));
    }
}

Server::~Server() {
    close(s_socket_);
    close(epoll_fd_);

    for (auto& [client_fd, pgsql_fd] : pgsql_sockets_) {
        close(pgsql_fd);
    }
}

/*
    Простая функция подключения к серверу PostgreSQL с помощью сокетов беркли по порту 5432.
    Нужна для присвоения каждому клиенту своего уникального дескриптора, т.к каждый клиент перед отправкой SQL запросов согласно сетевому протоколу PostgreSQL,
    сперва должен отправлять запросы на настройку сервера, например SSLRequest, затем предоставить данные на один из запросов аутентификации, затем получить от сервера AuthenticationOk и ReadyForQuery.
    Но так как для сервера PostgreSQL этот обмен данными нужно провести всего 1 раз, а клиентов может быть много и каждый из них отправляет эти данные,
    то появляется необходимость создавать для каждого клиента свое уникальное соединение с сервером PostgreSQL.
*/
int Server::ConnectToPGSQL() const {
    int pgsql_socket{socket(AF_INET, SOCK_STREAM, 0)};

    if (s_socket_ == -1) {
        close(pgsql_socket);
        throw std::runtime_error("Error creating socket: " + std::to_string(errno));
    }

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5432);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        close(pgsql_socket);
        throw std::runtime_error("Error converting IP address: " + std::to_string(errno));
    }

    auto casted_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (connect(pgsql_socket, casted_addr, sizeof(server_addr))) {
        close(pgsql_socket);
        throw std::runtime_error("Error connecting to postgresql server: " + std::to_string(errno));
    }

    return pgsql_socket;
}

/*
    Функция получения дескриптора сокета для моего сервера и установки для него специальных опций
    (опции говорят о том, что порт можно переиспользовать после каждого запуска программы)
*/
void Server::SetupSocket() {
    s_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (s_socket_ == -1) {
        throw std::runtime_error("Error creating socket: " + std::to_string(errno));
    }

    int opt{1};

    if (setsockopt(s_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("Error setting socket options: " + std::to_string(errno));
    }
}

/*
    Функция получения дескриптора epoll и добавления дескриптора моего сервера в список просматриваемых.
*/
void Server::SetupEpoll() {
    epoll_fd_ = epoll_create1(0);
    
    if (epoll_fd_ == -1) {
        throw std::runtime_error("Error creating epoll instance: " + std::to_string(errno));
    }

    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = s_socket_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, s_socket_, &event) == -1) {
        throw std::runtime_error("Error adding socket to epoll: " + std::to_string(errno));
    }
}

/*
    Функция принимающая новое соединение,
    в случае успешного принятия добавляет дескритор клиента в список просматриваемых дескрипторов в epoll.
    Устанавливает флаг EPOLLET для epoll, что означает что epoll будет работать в режиме edge-triggered,
    то есть будет возвращать только те дескрипторы, в которых были совершены события
*/
bool Server::AcceptNewConnection(epoll_event& event) const {
    struct sockaddr_in c_addr;
    socklen_t c_addr_len{sizeof(c_addr)};
    auto casted_c_addr{reinterpret_cast<struct sockaddr*>(&c_addr)};
    int c_socket{accept(s_socket_, casted_c_addr, &c_addr_len)};

    if (c_socket == -1) {
        std::cerr << "Error accepting connection: " << strerror(errno) << "\n";
        return false;
    } else {
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = c_socket;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, c_socket, &event) == -1) {
            std::cerr << "Error adding client socket to epoll: " << strerror(errno) << "\n";
            close(c_socket);
            return false;
        }
    }

    return true;
}

/*
    Функция проверки на то, является ли запрос SQL запросом, чтобы в дальнейшем логировать его в файл.
*/
bool Server::IsSQLRequest(std::string_view request) const {
    static const std::vector<std::string> tokens{
        "BEGIN", "COMMIT", "INSERT",
        "SELECT", "UPDATE", "DELETE"
    };

    for (const auto& token : tokens) {
        if (request.find(token) != std::string::npos) {
            return true;
        }
    }

    return false;
}

/*
    Функция пполучения подстроки с SQL запросом из строки, которая была передана в формате из сетевого протокола PostgreSQL
*/
std::string Server::GetSQLRequest(std::string_view request) const {
    static const std::vector<std::string> tokens{
        "BEGIN", "COMMIT", "INSERT",
        "SELECT", "UPDATE", "DELETE"
    };

    std::vector<int> positions;

    for (const auto& token : tokens) {
        if (auto pos{request.find(token)}; pos != std::string::npos) {
            positions.push_back(pos);
        }
    }

    request.remove_prefix(*std::min(positions.begin(), positions.end()));

    return std::string(request.substr(0, request.find('\0')));
}

/*
    Функция логирования запросов в файл.
*/
void Server::SaveLogs(std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string sql_req(std::move(GetSQLRequest(request)));

    log_file_ << sql_req << '\n';
}

/*
    Функция обработчик событий от клиентов,
    реализует передачу данных от клиента на сервер PostgreSQL и обратно,
    попутно логируя данные в файл.
*/
void Server::HandleClientEvent(epoll_event& event) {
    char buffer[max_buffer_size_];
    ssize_t bytes_read{recv(event.data.fd, buffer, max_buffer_size_, 0)};

    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
        close(event.data.fd);
        close(pgsql_sockets_[event.data.fd]);
        pgsql_sockets_.erase(event.data.fd);
    } else {
        SaveLogs(std::string(buffer, bytes_read));

        if (send(pgsql_sockets_[event.data.fd], buffer, bytes_read, 0) < 0) {
            throw std::runtime_error("Error sending query to PostgreSQL server: " + std::to_string(errno));
        }

        memset(buffer, 0, sizeof(buffer));

        bytes_read = recv(pgsql_sockets_[event.data.fd], buffer, max_buffer_size_, 0);

        if (bytes_read <= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, NULL);
            close(event.data.fd);
            close(pgsql_sockets_[event.data.fd]);
            pgsql_sockets_.erase(event.data.fd);
        } else {           
            if (send(event.data.fd, buffer, bytes_read, 0) < 0) {
                throw std::runtime_error("Error sending a response to a client: " + std::to_string(errno));
            }
        }
    }
}

/*
    Функция проверки на то, является ли запрос SSLRequest, чтобы в дальнейшем отклонить SSL соединение.
*/
bool Server::IsSSLRequest(char* buffer) const {
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

/*
    Функция передачи отказа от SSL соединения клиенту,
    так же можно было просто отключить SSL в файлах конфигурации PostgreSQL,
    тогда эта функция была бы не нужна, отказ отправлялся бы автоматически.
*/
void Server::DisableSSL(epoll_event& event) const {
    char buffer[max_buffer_size_];
    ssize_t bytes_read{recv(event.data.fd, buffer, max_buffer_size_, 0)};

    if (bytes_read > 0 && IsSSLRequest(buffer)) {
        if (send(event.data.fd, "N", 1, 0) < 0) {
            throw std::runtime_error("SSL disabling error: " + std::to_string(errno));
        }
    }
}

/*
    Функция ожидания новых событий или подключений.
    При успешном новом подключении, отправляется отказ от SSL,
    затем к дескриптору клиента привязывается уникадьный дескриптор сервера PostgreSQL (Для этого была использована хэш-таблица, unordered_map)
*/
void Server::EventLoop() {
    std::cout << "Waiting...\n";

    std::vector<struct epoll_event> events(max_events_);

    while (true) {
        int num_events{epoll_wait(epoll_fd_, events.data(), max_events_, -1)};

        for (int i{}; i < num_events; ++i) {
            if (events[i].data.fd == s_socket_) {
                if (AcceptNewConnection(events[i])) {
                    DisableSSL(events[i]);
                    pgsql_sockets_[events[i].data.fd] = ConnectToPGSQL();
                    std::cout << "Accepted the new connection.\n";
                }
            } else {
                HandleClientEvent(events[i]);
            }
        }

        events.clear();
        events.reserve(max_events_);
    }
}

/*
    Основная функция запуска сервера.
*/
void Server::Start() {
    SetupSocket();

    auto casted_s_addr{reinterpret_cast<struct sockaddr*>(&s_addr_)};

    if (bind(s_socket_, casted_s_addr, sizeof(s_addr_)) == -1) {
        throw std::runtime_error("Socket binding error!");
    }

    if (listen(s_socket_, SOMAXCONN) == -1) {
        throw std::runtime_error("Socket listening Error!");
    }

    SetupEpoll();
    EventLoop();
}
