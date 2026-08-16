#include "Protocol.hpp"
#include "Socket.hpp"

#include <gtest/gtest.h>
#include <ws2tcpip.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

    struct ConnectedSockets {
        Socket client;
        Socket server;
    };

    ConnectedSockets makeConnectedSockets()
    {
        Socket listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!listener.valid()) {
            throw std::runtime_error(chat::socketError("socket"));
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
            || listen(listener.get(), 1) == SOCKET_ERROR) {
            throw std::runtime_error(chat::socketError("create test listener"));
        }

        int addressSize = sizeof(address);
        if (getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address), &addressSize) == SOCKET_ERROR) {
            throw std::runtime_error(chat::socketError("getsockname"));
        }

        Socket client(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!client.valid()
            || connect(client.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            throw std::runtime_error(chat::socketError("connect"));
        }

        Socket server(accept(listener.get(), nullptr, nullptr));
        if (!server.valid()) {
            throw std::runtime_error(chat::socketError("accept"));
        }
        return {std::move(client), std::move(server)};
    }

    class ProtocolTest : public ::testing::Test {
    protected:
        WinsockSession winsock;
    };

    TEST_F(ProtocolTest, SendsAndReceivesPacket)
    {
        auto sockets = makeConnectedSockets();
        ASSERT_TRUE(chat::sendPacket(sockets.client.get(), chat::MessageType::chat, "hello"));

        chat::Packet packet;
        ASSERT_EQ(chat::receivePacket(sockets.server.get(), packet), chat::ReceiveResult::success);
        EXPECT_EQ(packet.type, chat::MessageType::chat);
        EXPECT_EQ(packet.payload, "hello");
    }

    TEST_F(ProtocolTest, KeepsBackToBackPacketsSeparate)
    {
        auto sockets = makeConnectedSockets();
        ASSERT_TRUE(chat::sendPacket(sockets.client.get(), chat::MessageType::chat, "first"));
        ASSERT_TRUE(chat::sendPacket(sockets.client.get(), chat::MessageType::command, "/users"));

        chat::Packet first;
        chat::Packet second;
        ASSERT_EQ(chat::receivePacket(sockets.server.get(), first), chat::ReceiveResult::success);
        ASSERT_EQ(chat::receivePacket(sockets.server.get(), second), chat::ReceiveResult::success);
        EXPECT_EQ(first.payload, "first");
        EXPECT_EQ(second.type, chat::MessageType::command);
        EXPECT_EQ(second.payload, "/users");
    }

    TEST_F(ProtocolTest, ReassemblesFragmentedPacket)
    {
        auto sockets = makeConnectedSockets();
        constexpr std::string_view payload = "fragmented";
        const std::uint32_t networkSize = htonl(static_cast<std::uint32_t>(payload.size()));
        std::array<char, 5> header{};
        header[0] = static_cast<char>(chat::MessageType::chat);
        std::memcpy(header.data() + 1, &networkSize, sizeof(networkSize));

        ASSERT_EQ(send(sockets.client.get(), header.data(), 2, 0), 2);
        ASSERT_EQ(send(sockets.client.get(), header.data() + 2, 3, 0), 3);
        for (const char character : payload) {
            ASSERT_EQ(send(sockets.client.get(), &character, 1, 0), 1);
        }

        chat::Packet packet;
        ASSERT_EQ(chat::receivePacket(sockets.server.get(), packet), chat::ReceiveResult::success);
        EXPECT_EQ(packet.payload, payload);
    }

    TEST_F(ProtocolTest, RejectsOversizedOutgoingPayload)
    {
        auto sockets = makeConnectedSockets();
        const std::string payload(chat::maxPayloadSize + 1, 'x');
        EXPECT_FALSE(chat::sendPacket(sockets.client.get(), chat::MessageType::chat, payload));
    }

    TEST_F(ProtocolTest, ReportsDisconnectDuringPacket)
    {
        auto sockets = makeConnectedSockets();
        const std::array<char, 2> partialHeader{static_cast<char>(chat::MessageType::chat), 0};
        ASSERT_EQ(send(sockets.client.get(), partialHeader.data(), static_cast<int>(partialHeader.size()), 0),
            static_cast<int>(partialHeader.size()));
        sockets.client.shutdownBoth();
        sockets.client.reset();

        chat::Packet packet;
        EXPECT_EQ(chat::receivePacket(sockets.server.get(), packet), chat::ReceiveResult::disconnected);
    }

} // namespace
