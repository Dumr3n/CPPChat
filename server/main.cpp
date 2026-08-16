#include "ChatServer.hpp"
#include "Protocol.hpp"
#include "Socket.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {
    std::atomic_bool interrupted = false;
    BOOL WINAPI onConsoleSignal(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
            interrupted = true;
            return TRUE;
        }
        return FALSE;
    }
    
    void usage() { 
        std::cout << "Usage: server [--bind IPv4] [--port 1024-65535]\n"; 
    }
}

int main(int argc, char* argv[])
{
    std::string address = "127.0.0.1";
    std::uint16_t port = chat::defaultPort;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--bind" && i + 1 < argc) {
                address = argv[++i];
            } else if (option == "--port" && i + 1 < argc) {
                const int value = std::stoi(argv[++i]);
                if (value < 1024 || value > 65535) {
                    throw std::invalid_argument("Port must be between 1024 and 65535");
                }
                port = static_cast<std::uint16_t>(value);
            } else if (option == "--help") { 
                usage(); 
                return EXIT_SUCCESS; 
            } else { 
                usage(); 
                return EXIT_FAILURE; 
            }
        }
        WinsockSession winsock;
        ChatServer server(address, port);
        SetConsoleCtrlHandler(onConsoleSignal, TRUE);
        std::jthread signalWatcher([&server] {
            while (!interrupted) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            server.stop();
        });
        server.run();
        interrupted = true;
    } catch (const std::exception& error) {
        interrupted = true;
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
