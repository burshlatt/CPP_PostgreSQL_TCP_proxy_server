#include <utility>
#include <unistd.h>

#include "unique_fd.h"

UniqueFD::UniqueFD(int fd) :
    _fd(fd)
{}

UniqueFD::UniqueFD(UniqueFD&& other) noexcept :
    _fd(other._fd)
{
    other._fd = -1;
}

UniqueFD::~UniqueFD() {
    Close();
}

UniqueFD& UniqueFD::operator=(UniqueFD&& other) noexcept {
    if (this != &other) {
        Close();

        _fd = std::exchange(other._fd, -1);
    }

    return *this;
}

int UniqueFD::Get() const {
    return _fd;
}

bool UniqueFD::Valid() const {
    return _fd >= 0;
}

UniqueFD::operator int() const {
    return _fd;
}

void UniqueFD::Close() {
    if (_fd >= 0) {
        close(_fd);

        _fd = -1;
    }
}
