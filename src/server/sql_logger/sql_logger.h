#ifndef SQL_LOGGER_H
#define SQL_LOGGER_H

#include <string>
#include <fstream>

/**
 * @class SQLLogger
 * @brief Логгер данных sql запросов в файл.
 */
class SQLLogger {
public:
    /**
     * @brief Конструктор логгера.
     * @param path Путь до файла логов.
     */
    SQLLogger(const std::string& path);

    ~SQLLogger() = default;

public:
    /**
     * @brief Сохраняет SQL-запрос в лог.
     * @param request SQL-запрос.
     */
    void SaveLogs(std::string_view request);

private:
    /**
     * @brief Проверяет, является ли запрос SQL-запросом.
     * @param request Буфер запроса.
     * @return true, если SQL-запрос.
     */
    bool IsSQLRequest(std::string_view request) const;

    /**
     * @brief Извлекает SQL-запрос из сетевого протокольного сообщения.
     * @param request Исходная строка.
     * @return SQL-запрос.
     */
    std::string_view GetSQLRequest(std::string_view request) const;

public:
    std::ofstream _log_file; ///< Файл логирования SQL-запросов.
};

#endif // SQL_LOGGER_H
