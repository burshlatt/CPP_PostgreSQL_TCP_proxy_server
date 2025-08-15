#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H

#include <string>
#include <fstream>

#include "../connection/connection.h"

/**
 * @brief Класс для логирования SQL-запросов клиентов и состояния соединений.
 * 
 * Logger сохраняет SQL-запросы клиентов в файл и выводит информацию о соединениях в терминал.
 */
class Logger {
public:
    /**
     * @brief Конструктор Logger.
     * 
     * Открывает лог-файл для записи и инициализирует адрес PostgreSQL сервера.
     * 
     * @param db_host Хост PostgreSQL сервера.
     * @param db_port Порт PostgreSQL сервера.
     * @param path Путь к файлу логов.
     * @throws std::invalid_argument Если файл не может быть открыт.
     */
    Logger(const std::string& db_host, int db_port, const std::string& path);
    
    /**
     * @brief Деструктор Logger.
     */
    ~Logger() = default;

public:
    /**
     * @brief Сохраняет SQL-запрос клиента в лог-файл.
     * 
     * Игнорирует запросы, которые не являются SQL-запросами (например, контрольные пакеты).
     * 
     * @param client_ep Информация о клиенте (IP и порт).
     * @param request SQL-запрос клиента в виде строки.
     */
    void SaveLogs(const Endpoint& client_ep, std::string_view request);

    /**
     * @brief Выводит информацию о соединении в терминал.
     * 
     * @param client_ep Информация о клиенте (IP и порт).
     * @param status Статус соединения (открыто/закрыто).
     */
    void PrintInTerminal(const Endpoint& client_ep, ConnectionStatus status);

private:
    /**
     * @brief Получает текущую дату и время в виде строки.
     * 
     * @return std::string Текущее время в формате YYYY-MM-DD HH:MM:SS.
     */
    std::string GetCurrentTimestamp();

    /**
     * @brief Проверяет, является ли запрос SQL-запросом.
     * 
     * @param request Запрос клиента.
     * @return true Если это SQL-запрос.
     * @return false В противном случае.
     */
    bool IsSQLRequest(std::string_view request) const;

    /**
     * @brief Извлекает SQL-запрос из пакета клиента.
     * 
     * @param request Пакет клиента.
     * @return std::string_view SQL-запрос без служебных байтов.
     */
    std::string_view GetSQLRequest(std::string_view request) const;

public:
    std::string _pgsql_host; ///< Хост PostgreSQL сервера.
    std::string _pgsql_port; ///< Порт PostgreSQL сервера.
    
    std::ofstream _log_file; ///< Поток для записи логов в файл.
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_LOGGER_LOGGER_H
