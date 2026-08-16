#pragma once

#include <winsock2.h>

#include <stdexcept>
#include <string>
#include <utility>

class WinsockSession {
public:
    WinsockSession()
    {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed with error " + std::to_string(result));
        }
    }

    ~WinsockSession() { 
        WSACleanup(); 
    }

    WinsockSession(const WinsockSession&) = delete;
    WinsockSession& operator=(const WinsockSession&) = delete;
};

class Socket {
public:
    Socket() = default;
    explicit Socket(SOCKET handle) noexcept : handle_(handle) 
    {
    }

    ~Socket() { 
        reset(); 
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : handle_(std::exchange(other.handle_, INVALID_SOCKET)) 
    {
    }

    Socket& operator=(Socket&& other) noexcept
    {
        if (this != &other) {
            reset(std::exchange(other.handle_, INVALID_SOCKET));
        }
        return *this;
    }

    [[nodiscard]] 
    SOCKET get() const noexcept { 
        return handle_; 
    }

    [[nodiscard]] 
    bool valid() const noexcept { 
        return handle_ != INVALID_SOCKET; 
    }

    void shutdownBoth() const noexcept
    {
        if (valid()) {
            ::shutdown(handle_, SD_BOTH);
        }
    }

    void reset(SOCKET handle = INVALID_SOCKET) noexcept
    {
        if (valid()) {
            closesocket(handle_);
        }
        handle_ = handle;
    }

private:
    SOCKET handle_ = INVALID_SOCKET;
};
