#include <iostream>
#include <cstring>

#include <sys/epoll.h>
#include <sys/socket.h>

#include "session.h"

Session::Session(UniqueFD&& pgsql_fd, UniqueFD&& client_fd, ModEventsCallback cb) :
    _pgsql_fd(std::move(pgsql_fd)),
    _client_fd(std::move(client_fd)),
    _mod_events_cb(cb)
{}

int Session::GetPGSQLFD() const noexcept {
    return _pgsql_fd;
}

int Session::GetClientFD() const noexcept {
    return _client_fd;
}

int Session::GetPeerFD(int fd) const noexcept {
    return fd == _client_fd ? _pgsql_fd : _client_fd;
}

std::string_view Session::GetDataToClient() const {
    return std::string_view(_client_send_buffer.data(), _client_send_buffer.size());
}

std::string_view Session::GetDataToPGSQL() const {
    return std::string_view(_pgsql_send_buffer.data(), _pgsql_send_buffer.size());
}

bool Session::IsPGSQLFD(int fd) const noexcept {
    return fd == _pgsql_fd;
}

bool Session::IsClientFD(int fd) const noexcept {
    return fd == _client_fd;
}

void Session::UpdateEpoll(int fd) {
    auto& buffer{IsClientFD(fd) ? _client_send_buffer : _pgsql_send_buffer};

    uint32_t events{EPOLLIN | EPOLLET};

    if (!buffer.empty()) {
        events |= EPOLLOUT;
    }

    _mod_events_cb(fd, events);
}

bool Session::TrySend(int fd) {
    auto& buffer{IsClientFD(fd) ? _client_send_buffer : _pgsql_send_buffer};

    while (!buffer.empty()) {
        ssize_t n{send(fd, buffer.data(), buffer.size(), MSG_NOSIGNAL)};
        
        if (n > 0) {
            buffer.erase(buffer.begin(), buffer.begin() + n);
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                UpdateEpoll(fd);

                return true;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "send() error to PostgreSQL: " << strerror(errno) << '\n';

            return false;
        }
    }

    UpdateEpoll(fd);

    return true;
}

bool Session::RecvAll(int fd) {
    auto& buffer{IsClientFD(fd) ? _pgsql_send_buffer : _client_send_buffer};
    constexpr size_t BUFFER_SIZE{4096};

    while (true) {
        char temp[BUFFER_SIZE];
        ssize_t n{recv(fd, temp, sizeof(temp), 0)};

        if (n > 0) {
            buffer.insert(buffer.end(), temp, temp + n);
        } else if (n == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "recv() error from client: " << strerror(errno) << '\n';

            return false;
        }
    }

    return true;
}
