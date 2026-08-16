#include "Protocol.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>

namespace chat {
    namespace {

        bool sendAll(SOCKET socket, const char* data, std::size_t size)
        {
            std::size_t sent = 0;
            while (sent < size) {
                const auto chunk = static_cast<int>(
                    (std::min)(size - sent, static_cast<std::size_t>((std::numeric_limits<int>::max)())));
                const int result = send(socket, data + sent, chunk, 0);
                if (result == SOCKET_ERROR || result == 0) {
                    return false;
                }
                sent += static_cast<std::size_t>(result);
            }
            return true;
        }

        ReceiveResult receiveAll(SOCKET socket, char* data, std::size_t size)
        {
            std::size_t received = 0;
            while (received < size) {
                const auto chunk = static_cast<int>((std::min)(size - received, static_cast<std::size_t>(INT_MAX)));
                const int result = recv(socket, data + received, chunk, 0);
                if (result == 0) {
                    return ReceiveResult::disconnected;
                }
                if (result == SOCKET_ERROR) {
                    return ReceiveResult::error;
                }
                received += static_cast<std::size_t>(result);
            }
            return ReceiveResult::success;
        }

    } // namespace

    bool sendPacket(SOCKET socket, MessageType type, std::string_view payload)
    {
        if (payload.size() > maxPayloadSize) {
            return false;
        }

        const std::uint32_t networkSize = htonl(static_cast<std::uint32_t>(payload.size()));
        std::array<char, 5> header{};
        header[0] = static_cast<char>(type);
        std::memcpy(header.data() + 1, &networkSize, sizeof(networkSize));

        return sendAll(socket, header.data(), header.size())
            && sendAll(socket, payload.data(), payload.size());
    }

    ReceiveResult receivePacket(SOCKET socket, Packet& packet)
    {
        std::array<char, 5> header{};
        auto result = receiveAll(socket, header.data(), header.size());
        if (result != ReceiveResult::success) {
            return result;
        }

        const auto rawType = static_cast<std::uint8_t>(header[0]);
        if (rawType < static_cast<std::uint8_t>(MessageType::hello)
            || rawType > static_cast<std::uint8_t>(MessageType::error)) {
            return ReceiveResult::invalid;
        }

        std::uint32_t networkSize{};
        std::memcpy(&networkSize, header.data() + 1, sizeof(networkSize));
        const std::uint32_t size = ntohl(networkSize);
        if (size > maxPayloadSize) {
            return ReceiveResult::invalid;
        }

        packet.type = static_cast<MessageType>(rawType);
        packet.payload.resize(size);
        return receiveAll(socket, packet.payload.data(), packet.payload.size());
    }

    std::string socketError(std::string_view operation)
    {
        return std::string(operation) + " failed (Winsock error " + std::to_string(WSAGetLastError()) + ')';
    }

} // namespace chat
