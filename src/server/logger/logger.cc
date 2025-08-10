#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "logger.h"

Logger::Logger(const std::string& path) {
    _log_file.open(path, std::ios::app);

    if (!_log_file.is_open()) {
        throw std::runtime_error("Server::Server() error opening file: " + std::to_string(errno));
    }
}

bool Logger::IsSQLRequest(std::string_view request) const {
    return !request.empty() && request[0] == 'Q';
}

std::string_view Logger::GetSQLRequest(std::string_view request) const {
    if (request.back() == '\0') {
        request.remove_suffix(1); // Убираем нулевой терминатор
    }

    return request.substr(5); // Убираем первые 5 байт (1 - Тип сообщения, 4 - размер сообщения)
}

void Logger::SaveLogs(const Endpoint& ep, std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string current_time{"[" + GetCurrentTime() + "] "};
    std::string client_info{"[client: " + ep.ip + ":" + std::to_string(ep.port) + "] "};
    std::string sql_req(GetSQLRequest(request));
    std::string result_str{current_time + client_info + sql_req};

    _log_file << result_str << '\n';
}

std::string Logger::GetCurrentTime() {
    auto now{std::chrono::system_clock::now()};
    std::time_t now_time{std::chrono::system_clock::to_time_t(now)};
    std::tm* local_time{std::localtime(&now_time)};

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

void Logger::PrintInTerminal(const Connection& conn) {
    std::string current_time{"[" + GetCurrentTime() + "] "};
    std::string connection_status{conn.conn_status == ConnectionStatus::K_OPEN ? "Connection open " : "Connection closed "};
    std::string fd_info{"(client_fd=" + std::to_string(conn.client.fd) + ", server_fd=" + std::to_string(conn.pgsql.fd) + "): "};
    std::string ip_info{"client " + conn.client.ip + ":" + std::to_string(conn.client.port) + " -> pgsql server " + conn.pgsql.ip + ":" + std::to_string(conn.pgsql.port)};
    std::string result_str{current_time + connection_status + fd_info + ip_info};

    std::cout << result_str << std::endl;
}
