#ifndef CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_UNIQUE_FD_UNIQUE_FD_H
#define CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_UNIQUE_FD_UNIQUE_FD_H

/**
 * @class UniqueFD
 * @brief RAII-обёртка для файлового дескриптора. Закрывает его в деструкторе.
 */
class UniqueFD {
public:
    /**
     * @brief Конструктор обертки для fd.
     * @param fd Дескриптор.
     */
    explicit UniqueFD(int fd = -1);

    /**
     * @brief Удаленный конструктор копирования для обертки fd.
     */
    UniqueFD(const UniqueFD&) = delete;

    /**
     * @brief Конструктор перемещения для обертки fd.
     * @param other rvalue ссылка на другой объект.
     */
    UniqueFD(UniqueFD&& other) noexcept;

    /**
     * @brief Деструктор обертки. Закрывает дескриптор.
     */
    ~UniqueFD();

    /**
     * @brief Удаленный оператор копирования-присвоения для обертки fd.
     */
    UniqueFD& operator=(const UniqueFD&) = delete;

    /**
     * @brief Оператор перемещения-присвоения для обертки fd.
     * @param other rvalue ссылка на другой объект.
     */
    UniqueFD& operator=(UniqueFD&& other) noexcept;

public:
    /**
     * @brief Явное преобразование в int (дескриптор).
     */
    operator int() const;

    /**
     * @brief Возвращает дескриптор.
     */
    int Get() const;

    /**
     * @brief Является ли дескриптор валидным.
     */
    bool Valid() const;

    /**
     * @brief Закрывает дескриптор вручную.
     */
    void Close();

private:
    int _fd; ///< Дескриптор сокета.
};

#endif // CPP_POSTGRESQL_TCP_PROXY_SERVER_SERVER_UNIQUE_FD_UNIQUE_FD_H
