#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>

std::vector<std::pair<std::string, SOCKET>> clients;
std::mutex clientMutex;

void broadcastMessage(const std::string& msg, SOCKET senderSocket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for ( auto [username, clientSocket] : clients) {
        
        if (clientSocket == senderSocket)
            continue;
        
        send(clientSocket, msg.c_str(), (int)msg.size(), 0);
    }
}

std::string receiveMessage(SOCKET clientSocket)
{
    char buffer[1024];
    int bytesReceived = recv(
        clientSocket,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytesReceived <= 0)
        return {};
    buffer[bytesReceived] = '\0';

    std::string msg = buffer;

    if (!msg.empty() && msg.back() == '\n')
        msg.pop_back();
    
    return msg;
}

void handleClient(SOCKET clientSocket)
{

    std::string username = receiveMessage(clientSocket);
    
    if (username.empty()) {
        std::cout << "Client disconnected without sending username" << std::endl;
        closesocket(clientSocket);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clients.push_back({username, clientSocket});
    }

    std::cout << "Username " << username << " connected!" << std::endl;
    

    char buffer[1024];

    while (true) {
        int bytesReceived = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::cout << "Received from " << username << ": " << buffer;
            std::string msg = username + ": " + buffer;
            broadcastMessage(msg, clientSocket);
        } else {
            std::cout << "Client disconnected without sending data" << std::endl;
            {
                std::lock_guard<std::mutex> lock(clientMutex);
                auto client = std::find_if(clients.begin(), clients.end(), [clientSocket](const auto& pair){return pair.second == clientSocket;});
                if (client != clients.end()) {
                    std::cout << "Remove " << client->first << std::endl;
                    clients.erase(client);
                }
            }
            break;
        }
    }

    closesocket(clientSocket);
}

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

    result = listen(serverSocket, SOMAXCONN);

    if (result == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server is listening on port 54000" << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed" << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return EXIT_FAILURE;
        }
        std::cout << "Attempt to connect" << std::endl;

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    

    return 0;
}