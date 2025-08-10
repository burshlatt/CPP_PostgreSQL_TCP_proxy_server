#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H

#include <string>
#include <fstream>

#include "../connection_info/connection_info.h"

/**
 * @class Logger
 * @brief Логгер данных sql запросов и информации о соединениях.
 */
class Logger {
public:
    /**
     * @brief Конструктор логгера.
     * @param path Путь до файла логов.
     */
    Logger(const std::string& path);

    ~Logger() = default;

public:
    /**
     * @brief Сохраняет SQL-запрос в лог.
     * @param ep структура с информацией о клиенте.
     * @param request SQL-запрос.
     */
    void SaveLogs(const Endpoint& ep, std::string_view request);

    /**
     * @brief Выводит информацию о соединении в терминал.
     * @param conn структура с информацией о соединении.
     */
    void PrintInTerminal(const Connection& conn);

private:
    /**
     * @brief Получение информации о текущей дате и времени.
     * @return Строка с текущей датой и временем.
     */
    std::string GetCurrentTime();

    /**
     * @brief Проверяет, является ли запрос SQL-запросом.
     * @param request буфер запроса.
     * @return true, если SQL-запрос.
     */
    bool IsSQLRequest(std::string_view request) const;

    /**
     * @brief Извлекает SQL-запрос из сетевого протокольного сообщения.
     * @param request исходная строка.
     * @return SQL-запрос.
     */
    std::string_view GetSQLRequest(std::string_view request) const;

public:
    std::ofstream _log_file; ///< Файл логирования SQL-запросов.
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H
