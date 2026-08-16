#pragma once

#include "Socket.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

class ChatServer {
public:
    ChatServer(std::string bindAddress, std::uint16_t port);
    ~ChatServer();
    void run();
    void stop();
private:
    struct Client { 
        std::uint64_t id{}; 
        std::string username; 
        Socket socket; 
        std::mutex sendMutex; 
    };
    void handleClient(std::shared_ptr<Client> client);
    void removeClient(const std::shared_ptr<Client>& client);
    void handleCommand(const std::shared_ptr<Client>& client, std::string_view command);
    bool sendTo(const std::shared_ptr<Client>& client, std::uint8_t type, std::string_view text);
    void broadcast(std::uint8_t type, std::string_view text, std::uint64_t excludedId = 0);
    std::vector<std::shared_ptr<Client>> snapshotClients();
    void log(std::string_view message);

    std::string bindAddress_;
    std::uint16_t port_;
    Socket listener_;
    std::atomic_bool stopping_{false};
    std::atomic_uint64_t nextClientId_{1};
    std::mutex clientsMutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<Client>> clients_;
    std::vector<std::jthread> clientThreads_;
    std::mutex logMutex_;
};
