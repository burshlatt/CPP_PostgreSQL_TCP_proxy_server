#include "sql_logger.h"

SQLLogger::SQLLogger(const std::string& path) {
    _log_file.open(path, std::ios::app);

    if (!_log_file.is_open()) {
        throw std::runtime_error("Server::Server() error opening file: " + std::to_string(errno));
    }
}

bool SQLLogger::IsSQLRequest(std::string_view request) const {
    return !request.empty() && request[0] == 'Q';
}

std::string_view SQLLogger::GetSQLRequest(std::string_view request) const {
    if (request.back() == '\0') {
        request.remove_suffix(1); // Убираем нулевой терминатор
    }

    return request.substr(5); // Убираем первые 5 байт (1 - Тип сообщения, 4 - размер сообщения)
}

void SQLLogger::SaveLogs(std::string_view request) {
    if (!IsSQLRequest(request)) {
        return;
    }

    std::string sql_req(GetSQLRequest(request));

    _log_file.write(sql_req.data(), sql_req.size());
    _log_file << std::endl;
}
