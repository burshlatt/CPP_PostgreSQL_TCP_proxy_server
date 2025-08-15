#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SERVER_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SERVER_H

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <sys/epoll.h>

#include "logger/logger.h"
#include "session/session.h"
#include "unique_fd/unique_fd.h"
#include "connection/connection.h"

/**
 * @class Server
 * @brief Класс для реализации асинхронного прокси-сервера с использованием epoll и подключением к PostgreSQL.
 *
 * Сервер принимает клиентские соединения, устанавливает соединение с PostgreSQL, проксирует данные
 * и логирует запросы. Основан на неблокирующем вводе-выводе и механизме epoll.
 */
class Server {
public:
    /**
     * @brief Конструктор сервера.
     * @param listen_port Порт, на котором сервер будет слушать входящие подключения.
     * @param db_host IP-адрес хоста PostgreSQL.
     * @param db_port Порт PostgreSQL.
     * @param log_file Путь к файлу логов.
     * @throw std::invalid_argument Если передан некорректный порт или хост.
     */
    Server(int listen_port, const std::string& db_host, int db_port, const std::string& log_file);

    /**
     * @brief Запускает сервер.
     *
     * Устанавливает обработчик сигналов, настраивает epoll, серверный сокет и запускает основной цикл обработки событий.
     */
    void Run();

private:
    /**
     * @brief Создает epoll-дескриптор и проверяет корректность.
     * @throw std::runtime_error Если epoll не удалось создать.
     */
    void SetupEpoll();

    /**
     * @brief Настраивает серверный сокет для прослушивания клиентских подключений.
     *
     * Сокет устанавливается в неблокирующий режим, привязывается к IP/порту, включается опция SO_REUSEADDR
     * и добавляется в epoll.
     * @throw std::runtime_error Если не удалось создать, настроить или привязать сокет.
     */
    void SetupServerSocket();

    /**
     * @brief Устанавливает подключение к PostgreSQL.
     *
     * Создает сокет, подключается к указанному хосту и порту PostgreSQL, переводит его в неблокирующий режим
     * и добавляет в epoll для отслеживания событий.
     * @return Объект UniqueFD с файловым дескриптором PostgreSQL.
     * @throw std::runtime_error Если не удалось создать или подключить сокет.
     */
    UniqueFD SetupPGSQLSocket();

    /**
     * @brief Основной цикл обработки событий epoll.
     *
     * Обрабатывает клиентские подключения и обмен данными между клиентами и PostgreSQL до тех пор,
     * пока не будет установлен флаг завершения (stop_flag).
     * @throw std::runtime_error Если epoll_wait вернет ошибку, отличную от EINTR.
     */
    void EventLoop();

    /**
     * @brief Обновляет маску событий для указанного файлового дескриптора в epoll.
     * @param fd Файловый дескриптор.
     * @param events Новая маска событий (EPOLLIN, EPOLLOUT и т.д.).
     */
    void UpdateEpollEvents(int fd, uint32_t events);

    /**
     * @brief Принимает новые клиентские подключения.
     *
     * Создает неблокирующий сокет для клиента, добавляет его в epoll,
     * открывает соединение с PostgreSQL и создает сессию.
     * В случае ошибок выводит сообщение в stderr.
     */
    void AcceptNewConnections();

    /**
     * @brief Закрывает сессию (клиент + PostgreSQL).
     * @param session Умный указатель на объект Session.
     *
     * Удаляет оба дескриптора из epoll, очищает хэштейблы сессий и эндпоинтов, логирует закрытие соединения.
     */
    void CloseSession(std::shared_ptr<Session> session);

    /**
     * @brief Обрабатывает событие epoll для конкретного дескриптора.
     * @param event Структура epoll_event, содержащая информацию о событии.
     *
     * Выполняет чтение/запись данных, проксирование между клиентом и PostgreSQL,
     * и логирование сообщений от клиента.
     */
    void HandleEvent(epoll_event& event);

    /**
     * @brief Проверяет корректность порта.
     * @param port Порт.
     * @return Корректный порт.
     * @throw std::invalid_argument Если порт вне диапазона (1-65535).
     */
    int CheckPort(int port);

    /**
     * @brief Проверяет корректность IP-адреса.
     * @param host IP-адрес в строковом формате.
     * @return Корректный IP-адрес.
     * @throw std::invalid_argument Если адрес некорректный.
     */
    std::string CheckHost(const std::string& host);

private:
    int _listen_port; ///< Порт для прослушивания клиентских соединений.
    std::string _db_host; ///< IP-адрес хоста PostgreSQL.
    int _db_port; ///< Порт PostgreSQL.
    Logger _logger; ///< Логгер для записи информации о соединениях и сообщениях.

    UniqueFD _proxy_fd{}; ///< Файловый дескриптор серверного сокета (слушающего).
    UniqueFD _epoll_fd{}; ///< Файловый дескриптор epoll.

    std::unordered_map<int, Endpoint> _fd_endpoint_ht; ///< Соотношение клиентский fd <-> структура Endpoint.
    std::unordered_map<int, std::shared_ptr<Session>> _fd_session_ht; ///< Соотношение fd <-> сессия.
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_SERVER_H
