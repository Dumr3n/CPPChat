#pragma once

#include <winsock2.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace chat {

constexpr std::uint32_t maxPayloadSize = 4096;
constexpr std::uint16_t defaultPort = 54000;

enum class MessageType : std::uint8_t {
    hello = 1,
    chat,
    command,
    system,
    users,
    error
};

struct Packet {
    MessageType type{};
    std::string payload;
};

enum class ReceiveResult { success, disconnected, error, invalid };

bool sendPacket(SOCKET socket, MessageType type, std::string_view payload);
ReceiveResult receivePacket(SOCKET socket, Packet& packet);
std::string socketError(std::string_view operation);

} // namespace chat

