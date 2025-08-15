#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "logger.h"

Logger::Logger(const std::string& db_host, int db_port, const std::string& path) :
    _pgsql_host(db_host),
    _pgsql_port(std::to_string(db_port))
{
    _log_file.open(path, std::ios::app);

    if (!_log_file.is_open()) {
        throw std::invalid_argument("Invalid file: " + path);
    }
}

bool Logger::IsSQLRequest(std::string_view request) const {
    return !request.empty() && request[0] == 'Q';
}

std::string_view Logger::GetSQLRequest(std::string_view request) const {
    if (request.back() == '\0') {
        request.remove_suffix(1);
    }

    return request.substr(5);
}

std::string Logger::GetCurrentTimestamp() {
    auto now{std::chrono::system_clock::now()};
    std::time_t now_time{std::chrono::system_clock::to_time_t(now)};
    std::tm* local_time{std::localtime(&now_time)};

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

void Logger::SaveLogs(const Endpoint& clinet_ep, std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string current_time{"[" + GetCurrentTimestamp() + "] "};
    std::string client_info{"[client: " + clinet_ep.ip + ":" + std::to_string(clinet_ep.port) + "] "};
    std::string sql_req(GetSQLRequest(request));
    std::string result_str{current_time + client_info + sql_req};

    _log_file << result_str << '\n';
}

void Logger::PrintInTerminal(const Endpoint& clinet_ep, ConnectionStatus status) {
    std::string current_time{"[" + GetCurrentTimestamp() + "] "};
    std::string connection_status{status == ConnectionStatus::K_OPEN ? "Connection open: " : "Connection closed: "};
    std::string ip_info{"client " + clinet_ep.ip + ":" + std::to_string(clinet_ep.port) + " -> pgsql server " + _pgsql_host + ":" + _pgsql_port};
    std::string result_str{current_time + connection_status + ip_info};

    std::cout << result_str << std::endl;
}
