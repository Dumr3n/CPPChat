#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>

int main()
{
    WSADATA wsaData{};

    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return EXIT_FAILURE;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create server socket" << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server socket created successfully" << std::endl;

    closesocket(serverSocket);
    WSACleanup();
    

    return 0;
}