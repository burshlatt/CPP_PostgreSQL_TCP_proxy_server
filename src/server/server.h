#ifndef TCP_PROXY_SERVER_SERVER_HPP
#define TCP_PROXY_SERVER_SERVER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <string_view>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>

/**
 * @class Server
 * @brief TCP-прокси сервер для обработки подключений и переадресации запросов на PostgreSQL.
 *
 * Сервер реализует работу с epoll, управляет клиентскими подключениями,
 * логирует SQL-запросы и обрабатывает отказ от SSL.
 */
class Server {
public:
    /**
     * @brief Конструктор сервера.
     * @param port Порт, на котором запускается сервер.
     */
    explicit Server(char* port);

    /**
     * @brief Деструктор сервера. Закрывает все открытые дескрипторы и освобождает ресурсы.
     */
    ~Server();

    /**
     * @brief Запускает сервер: инициализация, биндинг и основной цикл событий.
     */
    void Start();

private:
    /**
     * @brief Создаёт epoll.
     * @return Дескриптор epoll.
     */
    int SetupEpoll();

    /**
     * @brief Создаёт и настраивает сокет для прослушивания клиентских подключений.
     * @return Дескриптор сокета.
     */
    int SetupProxySocket();

    /**
     * @brief Устанавливает соединение с PostgreSQL-сервером.
     * @return Дескриптор подключения к PostgreSQL.
     */
    int SetupPGSQLSocket();

    /**
     * @brief Основной цикл обработки событий через epoll.
     */
    void EventLoop();

    /**
     * @brief Проверяет, является ли дескриптор клиентским.
     * @param fd Дескриптор для проверки.
     * @return true, если клиентский.
     */
    bool IsClientFD(int fd);

    /**
     * @brief Возвращает клиентский дескриптор по дескриптору PostgreSQL.
     * @param pgsql_fd Дескриптор PostgreSQL.
     * @return Дескриптор клиента, либо -1.
     */
    int GetClientFD(int pgsql_fd);

    /**
     * @brief Добавляет флаг EPOLLOUT для дескриптора.
     * @param fd Дескриптор.
     */
    void AddEpollOut(int fd);

    /**
     * @brief Удаляет флаг EPOLLOUT с дескриптора.
     * @param fd Дескриптор.
     */
    void RemoveEpollOut(int fd);

    /**
     * @brief Принимает новые входящие подключения.
     */
    void AcceptNewConnections();

    /**
     * @brief Закрывает сокет.
     * @param fd Дескриптор, связанный с клиентом или PostgreSQL.
     */
    void SafeCloseFD(int fd);

    /**
     * @brief Закрывает соединение с клиентом и PostgreSQL.
     * @param fd Дескриптор, связанный с клиентом или PostgreSQL.
     */
    void CloseConnection(int fd);

    /**
     * @brief Обрабатывает событие epoll.
     * @param event Структура события.
     */
    void HandleEvent(epoll_event& event);

    /**
     * @brief Проверяет, отключен ли SSL для клиента.
     * @param fd Дескриптор клиента.
     * @return true, если SSL отключен.
     */
    bool SSLDisabled(int fd) const;

    /**
     * @brief Проверяет, является ли запрос SQL-запросом.
     * @param request Буфер запроса.
     * @return true, если SQL-запрос.
     */
    bool IsSQLRequest(std::string_view request) const;

    /**
     * @brief Проверяет, является ли запрос SSL-запросом.
     * @param client_data Буфер с данными клиента.
     * @return true, если SSL-запрос.
     */
    bool IsSSLRequest(const std::vector<char>& client_data) const;

    /**
     * @brief Отклоняет SSL-запрос от клиента.
     * @param event epoll-событие клиента.
     */
    void DisableSSL(epoll_event& event);

    /**
     * @brief Сохраняет SQL-запрос в лог.
     * @param request SQL-запрос.
     */
    void SaveLogs(std::string_view request);

    /**
     * @brief Извлекает SQL-запрос из сетевого протокольного сообщения.
     * @param request Исходная строка.
     * @return SQL-запрос.
     */
    std::string_view GetSQLRequest(std::string_view request) const;

    /**
     * @brief Пытается отправить данные, если есть в буфере.
     * @param fd Дескриптор для отправки.
     * @return true, если успешно или частично отправлено.
     */
    bool TrySend(int fd);

    /**
     * @brief Отправляет данные, добавляя их в буфер отправки.
     * @param fd Дескриптор.
     * @param data Данные для отправки.
     * @return true, если успешно добавлено и передано.
     */
    bool SendBuffer(int fd, const std::vector<char>& data);

    /**
     * @brief Принимает все доступные данные с сокета.
     * @param fd Дескриптор.
     * @param buffer Буфер, куда пишутся данные.
     * @return Количество прочитанных байт.
     */
    ssize_t RecvAll(int fd, std::vector<char>& buffer);

private:
    unsigned _port; ///< Порт для прослушивания.

    int _proxy_fd{}; ///< Дескриптор сокета прокси-сервера.
    int _epoll_fd{}; ///< Дескриптор epoll.

    std::ofstream _log_file; ///< Файл логирования SQL-запросов.
    
    struct sockaddr_in _s_addr; ///< Структура адреса сервера.

    std::unordered_set<int> _ssl_disabled_clients_set; ///< Множество клиентов с отключенным SSL.
    std::unordered_map<int, int> _client_psql_sockets_ht; ///< Соответствие клиент ↔ PostgreSQL.
    std::unordered_map<int, std::vector<char>> _send_buffers_ht; ///< Буферы отправки по дескрипторам.
};

#endif // TCP_PROXY_SERVER_SERVER_HPP
