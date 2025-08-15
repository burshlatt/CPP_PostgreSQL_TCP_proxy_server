#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SESSION_SESSION_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SESSION_SESSION_H

#include <string>
#include <vector>
#include <functional>

#include "../unique_fd/unique_fd.h"

/**
 * @brief Класс, представляющий сессию между клиентским сокетом и сокетом PostgreSQL.
 * 
 * Сессия инкапсулирует два сокета и буферы для проксирования данных между ними.
 * Также управляет событиями epoll через коллбэк ModEventsCallback.
 */
class Session {
public:
    /// Тип коллбэка для обновления событий epoll (fd и новые события).
    using ModEventsCallback = std::function<void(int fd, uint32_t events)>;

public:
    /**
     * @brief Конструктор сессии.
     * 
     * @param pgsql_fd Сокет PostgreSQL.
     * @param client_fd Клиентский сокет.
     * @param cb Коллбэк для модификации событий epoll.
     */
    Session(UniqueFD&& pgsql_fd, UniqueFD&& client_fd, ModEventsCallback cb);

    /**
     * @brief Получить дескриптор сокета PostgreSQL.
     * @return int Дескриптор PostgreSQL.
     */
    int GetPGSQLFD() const noexcept;

    /**
     * @brief Получить дескриптор клиентского сокета.
     * @return int Дескриптор клиента.
     */
    int GetClientFD() const noexcept;

    /**
     * @brief Получить дескриптор "пиринга" для данного fd.
     * 
     * Если fd — клиентский, вернет fd PostgreSQL, и наоборот.
     * 
     * @param fd Исходный дескриптор.
     * @return int Дескриптор противоположного сокета.
     */
    int GetPeerFD(int fd) const noexcept;

    /**
     * @brief Получить данные, готовые для отправки в PostgreSQL.
     * @return std::string_view Данные из буфера PostgreSQL.
     */
    std::string_view GetDataToPGSQL() const;

    /**
     * @brief Получить данные, готовые для отправки клиенту.
     * @return std::string_view Данные из клиентского буфера.
     */
    std::string_view GetDataToClient() const;

    /**
     * @brief Проверяет, является ли fd сокетом PostgreSQL.
     * @param fd Дескриптор.
     * @return true Если fd — PostgreSQL.
     * @return false Иначе.
     */
    bool IsPGSQLFD(int fd) const noexcept;

    /**
     * @brief Проверяет, является ли fd клиентским сокетом.
     * @param fd Дескриптор.
     * @return true Если fd — клиентский.
     * @return false Иначе.
     */
    bool IsClientFD(int fd) const noexcept;

public:
    /**
     * @brief Пытается отправить все данные из буфера на указанный fd.
     * 
     * Если отправка невозможна (EAGAIN), вызывает UpdateEpoll.
     * 
     * @param fd Дескриптор для отправки.
     * @return true Если данные отправлены или ждут повторной попытки.
     * @return false Если произошла фатальная ошибка отправки.
     */
    bool TrySend(int fd);

    /**
     * @brief Считывает все доступные данные с указанного fd.
     * 
     * Читает данные и помещает их в буфер противоположного сокета.
     * 
     * @param fd Дескриптор для чтения.
     * @return true Если данные успешно считаны или достигнут EAGAIN.
     * @return false Если соединение закрыто или произошла ошибка.
     */
    bool RecvAll(int fd);

    /**
     * @brief Обновляет события epoll для указанного fd.
     * 
     * Добавляет EPOLLOUT, если в буфере есть данные для отправки.
     * 
     * @param fd Дескриптор, для которого обновляются события.
     */
    void UpdateEpoll(int fd);

private:
    UniqueFD _pgsql_fd; ///< Сокет PostgreSQL.
    UniqueFD _client_fd; ///< Клиентский сокет.

    ModEventsCallback _mod_events_cb; ///< Коллбэк для обновления событий epoll.

    std::vector<char> _pgsql_send_buffer; ///< Буфер для данных PostgreSQL.
    std::vector<char> _client_send_buffer; ///< Буфер для данных клиента.
};


#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SESSION_SESSION_H
