#include "ChatServer.hpp"
#include "Protocol.hpp"
#include <ws2tcpip.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
    constexpr std::size_t maxUsernameLength = 24;
    bool validUsername(std::string_view name) {
        return !name.empty() && name.size() <= maxUsernameLength
            && std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return std::isalnum(c) != 0 || c == '_' || c == '-';
            });
    }
}

ChatServer::ChatServer(std::string address, std::uint16_t port)
    : bindAddress_(std::move(address)), port_(port) 
    {
    }

ChatServer::~ChatServer()
{ 
    stop(); 
}

void ChatServer::run()
{
    listener_.reset(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!listener_.valid()) {
        throw std::runtime_error(chat::socketError("socket"));
    }
    BOOL reuse = TRUE;
    setsockopt(listener_.get(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    
    if (
        inet_pton(AF_INET, bindAddress_.c_str(), &address.sin_addr) != 1
    ) {
        throw std::runtime_error("Invalid bind address: " + bindAddress_);
    }

    if (
        bind(listener_.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
    ) {
        throw std::runtime_error(chat::socketError("bind"));
    }

    if (
        listen(listener_.get(), SOMAXCONN) == SOCKET_ERROR
    ) {
        throw std::runtime_error(chat::socketError("listen"));
    }
    log("Listening on " + bindAddress_ + ':' + std::to_string(port_));

    while (!stopping_) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(listener_.get(), &reads);
        timeval timeout{0, 250'000};
        const int ready = select(0, &reads, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            if (!stopping_) {
                throw std::runtime_error(chat::socketError("select"));
            }
            break;
        }
        if (ready == 0) {
            continue;
        }
        Socket accepted(accept(listener_.get(), nullptr, nullptr));
        if (!accepted.valid()) {
            if (!stopping_) {
                log(chat::socketError("accept"));
            }
            continue;
        }
        auto client = std::make_shared<Client>();
        client->id = nextClientId_++;
        client->socket = std::move(accepted);
        clientThreads_.emplace_back(
            [this, client] { 
                handleClient(client); 
            }
        );
    }
}

void ChatServer::stop()
{
    if (stopping_.exchange(true)) return;
    listener_.shutdownBoth();
    listener_.reset();
    for (const auto& client : snapshotClients()) client->socket.shutdownBoth();
    clientThreads_.clear();
}

void ChatServer::handleClient(std::shared_ptr<Client> client)\
{
    chat::Packet packet;
    if (chat::receivePacket(client->socket.get(), packet) != chat::ReceiveResult::success
        || packet.type != chat::MessageType::hello || !validUsername(packet.payload)) {
        sendTo(client, static_cast<std::uint8_t>(chat::MessageType::error),
            "Username must be 1-24 letters, digits, '-' or '_'.");
        return;
    }
    {
        std::lock_guard lock(clientsMutex_);
        const bool duplicate = std::any_of(clients_.begin(), clients_.end(), [&](const auto& item) {
            return item.second->username == packet.payload;
        });
        if (duplicate) {
            sendTo(client, static_cast<std::uint8_t>(chat::MessageType::error), "Username already in use.");
            return;
        }
        client->username = packet.payload;
        clients_.emplace(client->id, client);
    }
    log(client->username + " connected");
    sendTo(client, static_cast<std::uint8_t>(chat::MessageType::system),
        "Welcome, " + client->username + ". Type /help for commands.");
    broadcast(static_cast<std::uint8_t>(chat::MessageType::system), client->username + " joined.", client->id);

    while (!stopping_) {
        const auto result = chat::receivePacket(client->socket.get(), packet);
        if (result != chat::ReceiveResult::success) {
            if (result == chat::ReceiveResult::invalid)
                sendTo(client, static_cast<std::uint8_t>(chat::MessageType::error), "Invalid packet.");
            break;
        }
        if (packet.type == chat::MessageType::chat && !packet.payload.empty()) {
            log(client->username + ": " + packet.payload);
            broadcast(static_cast<std::uint8_t>(chat::MessageType::chat), client->username + ": " + packet.payload);
        } else if (packet.type == chat::MessageType::command) {
            handleCommand(client, packet.payload);
        }
    }
    removeClient(client);
}

void ChatServer::removeClient(const std::shared_ptr<Client>& client)
{
    bool removed;
    { 
        std::lock_guard lock(clientsMutex_); 
        removed = clients_.erase(client->id) != 0; 
    }
    if (removed) {
        log(client->username + " disconnected");
        broadcast(static_cast<std::uint8_t>(chat::MessageType::system), client->username + " left.", client->id);
    }
}

void ChatServer::handleCommand(const std::shared_ptr<Client>& client, std::string_view command)
{
    const auto system = static_cast<std::uint8_t>(chat::MessageType::system);
    const auto error = static_cast<std::uint8_t>(chat::MessageType::error);
    if (command == "/help") {
        sendTo(client, system, "Commands: /help, /users, /nick <name>, /me <action>, /quit");
    } else if (command == "/users") {
        const auto clients = snapshotClients();
        std::ostringstream out;
        out << "Online (" << clients.size() << "): ";
        for (std::size_t i = 0; i < clients.size(); ++i) { 
            if (i) {
                out << ", ";
            } 
            out << clients[i]->username; 
        }
        sendTo(client, static_cast<std::uint8_t>(chat::MessageType::users), out.str());
    } else if (command.starts_with("/me ") && command.size() > 4) {
        broadcast(system, "* " + client->username + ' ' + std::string(command.substr(4)));
    } else if (command.starts_with("/nick ")) {
        const std::string newName(command.substr(6));
        if (!validUsername(newName)) { 
            sendTo(client, error, "Invalid username."); 
            return; 
        }
        std::string oldName;
        {
            std::lock_guard lock(clientsMutex_);
            const bool duplicate = std::any_of(clients_.begin(), clients_.end(), [&](const auto& item) {
                return item.first != client->id && item.second->username == newName;
            });
            if (duplicate) { 
                sendTo(client, error, "Username already in use."); 
                return; 
            }
            oldName = std::exchange(client->username, newName);
        }
        broadcast(system, oldName + " is now " + newName + '.');
    } else {
        sendTo(client, error, "Unknown command. Type /help.");
    }
}

bool ChatServer::sendTo(const std::shared_ptr<Client>& client, std::uint8_t type, std::string_view text)
{
    std::lock_guard lock(client->sendMutex);
    return chat::sendPacket(client->socket.get(), static_cast<chat::MessageType>(type), text);
}

void ChatServer::broadcast(std::uint8_t type, std::string_view text, std::uint64_t excluded)
{
    for (const auto& client : snapshotClients()) {
        if (client->id != excluded && !sendTo(client, type, text)) {
            client->socket.shutdownBoth();
        }
    }
}

std::vector<std::shared_ptr<ChatServer::Client>> ChatServer::snapshotClients()
{
    std::lock_guard lock(clientsMutex_);
    std::vector<std::shared_ptr<Client>> result;
    result.reserve(clients_.size());
    for (const auto& item : clients_) {
        result.push_back(item.second);
    }
    return result;
}

void ChatServer::log(std::string_view message)
{
    std::lock_guard lock(logMutex_);
    std::cout << "[server] " << message << '\n';
}
