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

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(54000);

    result = bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));

    if (result == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server bound to port 54000" << std::endl;

    closesocket(serverSocket);
    WSACleanup();
    

    return 0;
}