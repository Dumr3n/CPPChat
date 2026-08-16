#include "Protocol.hpp"
#include "Socket.hpp"

#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
    std::mutex consoleMutex;
    std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t value = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
        localtime_s(&local, &value);
        std::ostringstream out;
        out << std::put_time(&local, "%H:%M");
        return out.str();
    }

    void printLine(std::string_view text) {
        std::lock_guard lock(consoleMutex);
        std::cout << "\r[" << timestamp() << "] " << text << "\n> " << std::flush;
    }

    void usage() { 
        std::cout << "Usage: client [--host IPv4] [--port 1-65535] [--name USERNAME]\n"; 
    }
}

int main(int argc, char* argv[])
{
    std::string host = "127.0.0.1";
    std::string username;
    std::uint16_t port = chat::defaultPort;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--host" && i + 1 < argc) {
                host = argv[++i];
            } else if (option == "--name" && i + 1 < argc) {
                username = argv[++i];
            } else if (option == "--port" && i + 1 < argc) {
                const int value = std::stoi(argv[++i]);
                if (value < 1024 || value > 65535) {
                    throw std::invalid_argument("Port must be between 1024 and 65535");
                }
                port = static_cast<std::uint16_t>(value);
            } else if (option == "--help") { 
                usage(); 
                return EXIT_SUCCESS; 
            }
            else { 
                usage(); 
                return EXIT_FAILURE; 
            }
        }
        if (username.empty()) {
            std::cout << "Username: ";
            std::getline(std::cin, username);
        }

        WinsockSession winsock;
        Socket socketHandle(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!socketHandle.valid()) {
            throw std::runtime_error(chat::socketError("socket"));
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        
        if (
            inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1
        ) {
            throw std::runtime_error("Invalid server address: " + host);
        }
        
        if (
            connect(socketHandle.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
        ) {
            throw std::runtime_error(chat::socketError("connect"));
        }

        if (
            !chat::sendPacket(socketHandle.get(), chat::MessageType::hello, username)
        ) {
            throw std::runtime_error(chat::socketError("send"));
        }

        std::atomic_bool connected = true;
        std::jthread receiver([&] {
            chat::Packet packet;
            while (connected) {
                const auto result = chat::receivePacket(socketHandle.get(), packet);
                if (result != chat::ReceiveResult::success) {
                    break;
                }
                const std::string prefix = packet.type == chat::MessageType::error ? "Error: " : "";
                printLine(prefix + packet.payload);
                if (packet.type == chat::MessageType::error && packet.payload.find("Username") != std::string::npos)
                    break;
            }
            connected = false;
        });

        { 
            std::lock_guard lock(consoleMutex); 
            std::cout << "Connected to " << host << ':' << port << "\n> " << std::flush; 
        }
        std::string line;
        while (connected && std::getline(std::cin, line)) {
            if (line == "/quit") {
                break;
            }
            if (line.empty()) { 
                std::cout << "> " << std::flush; 
                continue; 
            }
            const auto type = line.front() == '/' ? chat::MessageType::command : chat::MessageType::chat;
            if (!chat::sendPacket(socketHandle.get(), type, line)) { 
                printLine("Send failed."); 
                break; 
            }
            std::lock_guard lock(consoleMutex);
            std::cout << "> " << std::flush;
        }
        connected = false;
        socketHandle.shutdownBoth();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
