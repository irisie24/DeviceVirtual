#include "Mqtt.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <stdexcept>

namespace
{
constexpr std::uint8_t kConnect = 0x10;
constexpr std::uint8_t kConnAck = 0x20;
constexpr std::uint8_t kPublish = 0x30;
constexpr std::uint8_t kSubscribe = 0x80;
constexpr std::uint8_t kSubAck = 0x90;
constexpr std::uint8_t kUnsubscribe = 0xA0;
constexpr std::uint8_t kUnsubAck = 0xB0;
constexpr std::uint8_t kPingReq = 0xC0;
constexpr std::uint8_t kPingResp = 0xD0;
constexpr std::uint8_t kDisconnect = 0xE0;

std::once_flag g_wsaOnce;
bool g_wsaReady = false;

void encodeRemainingLength(std::vector<std::uint8_t>& packet, std::uint32_t value)
{
    do
    {
        std::uint8_t byte = value % 128;
        value /= 128;

        if (value > 0)
        {
            byte |= 0x80;
        }

        packet.push_back(byte);
    } while (value > 0);
}

bool ensureWinsockReady()
{
    std::call_once(g_wsaOnce, []() {
        WSADATA wsaData{};
        g_wsaReady = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    });

    return g_wsaReady;
}

bool writeSocket(SOCKET socketFd, const std::vector<std::uint8_t>& data)
{
    std::size_t offset = 0;
    while (offset < data.size())
    {
        const int sent = ::send(socketFd,
                                reinterpret_cast<const char*>(data.data() + offset),
                                static_cast<int>(data.size() - offset),
                                0);
        if (sent == SOCKET_ERROR)
        {
            const int errorCode = WSAGetLastError();
            if (errorCode == WSAEINTR)
            {
                continue;
            }
            return false;
        }

        if (sent == 0)
        {
            return false;
        }

        offset += static_cast<std::size_t>(sent);
    }

    return true;
}

bool readExactlyFromSocket(SOCKET socketFd, std::vector<std::uint8_t>& buffer, std::size_t expectedSize)
{
    while (buffer.size() < expectedSize)
    {
        std::vector<std::uint8_t> chunk(512, 0);
        const auto toRead = std::min<std::size_t>(chunk.size(), expectedSize - buffer.size());
        const int received = ::recv(socketFd,
                                    reinterpret_cast<char*>(chunk.data()),
                                    static_cast<int>(toRead),
                                    0);
        if (received == SOCKET_ERROR)
        {
            const int errorCode = WSAGetLastError();
            if (errorCode == WSAEINTR)
            {
                continue;
            }
            return false;
        }

        if (received == 0)
        {
            return false;
        }

        buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + static_cast<std::size_t>(received));
    }

    return true;
}

std::vector<std::uint8_t> encodeStringPayload(const std::string& value)
{
    std::vector<std::uint8_t> packet;
    const auto length = static_cast<std::uint16_t>(value.size());
    packet.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));
    packet.push_back(static_cast<std::uint8_t>(length & 0xFF));
    packet.insert(packet.end(), value.begin(), value.end());
    return packet;
}
}

MqttClient::MqttClient()
{
    isConnected_ = false;
    socketFd_ = -1;
    qos_ = 1;
    timeout_ = 30;
    stopReader_ = false;
    nextPacketId_ = 1;
}

MqttClient::MqttClient(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback)
{
    brokerAddress_ = brokerAddress;
    port_ = port;
    clientId_ = clientId;
    username_ = username;
    password_ = password;
    callback_ = callback;
    isConnected_ = false;
    socketFd_ = -1;
    qos_ = 1;
    timeout_ = 30;
    stopReader_ = false;
    nextPacketId_ = 1;
}

void MqttClient::Initialize(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback)
{
    brokerAddress_ = brokerAddress;
    port_ = port;
    clientId_ = clientId;
    username_ = username;
    password_ = password;
    callback_ = callback;
    isConnected_ = false;
    socketFd_ = -1;
    qos_ = 1;
    timeout_ = 30;
    stopReader_ = false;
    nextPacketId_ = 1;
}

MqttClient::~MqttClient()
{
    disconnect();
}

void MqttClient::readerLoop()
{
    while (!stopReader_.load())
    {
        std::vector<std::uint8_t> fixedHeader;
        if (!readExactlyFromSocket(static_cast<SOCKET>(socketFd_), fixedHeader, 1))
        {
            break;
        }

        if (fixedHeader.empty())
        {
            continue;
        }

        std::vector<std::uint8_t> remainingLengthBytes;
        std::uint32_t remainingLength = 0;
        std::uint32_t multiplier = 1;

        for (int i = 0; i < 4; ++i)
        {
            std::uint8_t byte = 0;
            if (recv(static_cast<SOCKET>(socketFd_), reinterpret_cast<char*>(&byte), 1, 0) <= 0)
            {
                return;
            }

            remainingLengthBytes.push_back(byte);
            remainingLength += (static_cast<std::uint32_t>(byte & 0x7F) * multiplier);
            if ((byte & 0x80) == 0)
            {
                break;
            }
            multiplier *= 128;
        }

        std::vector<std::uint8_t> payload;
        if (remainingLength > 0)
        {
            if (!readExactlyFromSocket(static_cast<SOCKET>(socketFd_), payload, remainingLength))
            {
                break;
            }
        }

        std::vector<std::uint8_t> packet;
        packet.push_back(fixedHeader[0]);
        packet.insert(packet.end(), remainingLengthBytes.begin(), remainingLengthBytes.end());
        packet.insert(packet.end(), payload.begin(), payload.end());

        const std::uint8_t messageType = fixedHeader[0] & 0xF0;

        if (messageType == kConnAck)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            isConnected_ = true;
            connectedCv_.notify_all();
            if (callback_ != nullptr)
            {
                callback_->onConnected();
            }
            continue;
        }

        if (messageType == kPublish)
        {
            parseIncomingPacket(packet);
        }
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        isConnected_ = false;
    }

    if (callback_ != nullptr)
    {
        callback_->onConnectionLost();
    }
}

bool MqttClient::writeAll(const std::vector<std::uint8_t>& data)
{
    if (socketFd_ < 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(socketMutex_);
    return writeSocket(static_cast<SOCKET>(socketFd_), data);
}

bool MqttClient::readExactly(std::vector<std::uint8_t>& buffer, std::size_t expectedSize)
{
    return readExactlyFromSocket(static_cast<SOCKET>(socketFd_), buffer, expectedSize);
}

std::vector<std::uint8_t> MqttClient::encodeString(const std::string& value) const
{
    return encodeStringPayload(value);
}

std::vector<std::uint8_t> MqttClient::buildConnectPacket(int qos, int keepAliveSeconds)
{
    (void)qos;
    std::vector<std::uint8_t> payload;
    payload.push_back(0x00);
    payload.push_back(0x04);
    payload.push_back('M');
    payload.push_back('Q');
    payload.push_back('T');
    payload.push_back('T');
    payload.push_back(0x04);

    std::uint8_t connectFlags = 0x02;
    if (!username_.empty())
    {
        connectFlags |= 0x80;
    }
    if (!password_.empty())
    {
        connectFlags |= 0x40;
    }
    payload.push_back(connectFlags);
    payload.push_back(static_cast<std::uint8_t>((keepAliveSeconds >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(keepAliveSeconds & 0xFF));

    const auto clientId = encodeStringPayload(clientId_);
    payload.insert(payload.end(), clientId.begin(), clientId.end());

    if (!username_.empty())
    {
        const auto username = encodeStringPayload(username_);
        payload.insert(payload.end(), username.begin(), username.end());
    }

    if (!password_.empty())
    {
        const auto password = encodeStringPayload(password_);
        payload.insert(payload.end(), password.begin(), password.end());
    }

    std::vector<std::uint8_t> packet;
    packet.push_back(kConnect);
    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<std::uint8_t> MqttClient::buildPublishPacket(const std::string& topic, const std::string& message, int qos)
{
    std::vector<std::uint8_t> packet;
    std::uint8_t flags = 0x00;
    if (qos > 0)
    {
        flags = 0x02;
    }
    packet.push_back(static_cast<std::uint8_t>(kPublish | flags));

    std::vector<std::uint8_t> payload;
    const auto topicPayload = encodeStringPayload(topic);
    payload.insert(payload.end(), topicPayload.begin(), topicPayload.end());

    if (qos > 0)
    {
        const auto packetId = static_cast<std::uint16_t>(nextPacketId_.fetch_add(1, std::memory_order_relaxed));
        payload.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
        payload.push_back(static_cast<std::uint8_t>(packetId & 0xFF));
    }

    payload.insert(payload.end(), message.begin(), message.end());
    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<std::uint8_t> MqttClient::buildSubscribePacket(const std::string& topic, int qos)
{
    std::vector<std::uint8_t> packet;
    packet.push_back(kSubscribe | 0x02);

    const auto packetId = static_cast<std::uint16_t>(nextPacketId_.fetch_add(1, std::memory_order_relaxed));
    std::vector<std::uint8_t> payload;
    payload.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(packetId & 0xFF));

    const auto topicPayload = encodeStringPayload(topic);
    payload.insert(payload.end(), topicPayload.begin(), topicPayload.end());
    payload.push_back(static_cast<std::uint8_t>(qos));

    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<std::uint8_t> MqttClient::buildUnsubscribePacket(const std::string& topic)
{
    std::vector<std::uint8_t> packet;
    packet.push_back(kUnsubscribe | 0x02);

    const auto packetId = static_cast<std::uint16_t>(nextPacketId_.fetch_add(1, std::memory_order_relaxed));
    std::vector<std::uint8_t> payload;
    payload.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(packetId & 0xFF));

    const auto topicPayload = encodeStringPayload(topic);
    payload.insert(payload.end(), topicPayload.begin(), topicPayload.end());

    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

void MqttClient::parseIncomingPacket(const std::vector<std::uint8_t>& packet)
{
    if (packet.size() < 5 || callback_ == nullptr)
    {
        return;
    }

    const std::uint8_t messageType = packet[0] & 0xF0;
    if (messageType != kPublish)
    {
        return;
    }

    std::size_t idx = 1;
    std::uint32_t remainingLength = 0;
    std::uint32_t multiplier = 1;
    while (true)
    {
        if (idx >= packet.size())
        {
            return;
        }

        const std::uint8_t byte = packet[idx++];
        remainingLength += (static_cast<std::uint32_t>(byte & 0x7F) * multiplier);
        if ((byte & 0x80) == 0)
        {
            break;
        }
        multiplier *= 128;
    }

    if (idx + 2 > packet.size())
    {
        return;
    }

    const std::uint16_t topicLength = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[idx]) << 8) | static_cast<std::uint16_t>(packet[idx + 1]));
    idx += 2;

    if (idx + topicLength > packet.size())
    {
        return;
    }

    const std::string topic(packet.begin() + static_cast<std::ptrdiff_t>(idx), packet.begin() + static_cast<std::ptrdiff_t>(idx + topicLength));
    idx += topicLength;

    const std::uint8_t qosFlags = packet[0] & 0x06;
    if (qosFlags != 0)
    {
        idx += 2;
    }

    std::string message(packet.begin() + static_cast<std::ptrdiff_t>(idx), packet.end());
    callback_->onMessageReceived(topic, message);
}

void MqttClient::connect(int qos, int timeout)
{
    qos_ = qos;
    timeout_ = timeout;

    if (!ensureWinsockReady())
    {
        return;
    }

    if (socketFd_ >= 0)
    {
        disconnect();
    }

    socketFd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socketFd_ == static_cast<int>(INVALID_SOCKET))
    {
        socketFd_ = -1;
        return;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port_);
    if (getaddrinfo(brokerAddress_.c_str(), portString.c_str(), &hints, &result) != 0)
    {
        closesocket(static_cast<SOCKET>(socketFd_));
        socketFd_ = -1;
        return;
    }

    bool connected = false;
    for (addrinfo* info = result; info != nullptr; info = info->ai_next)
    {
        if (::connect(static_cast<SOCKET>(socketFd_), info->ai_addr, static_cast<int>(info->ai_addrlen)) == 0)
        {
            connected = true;
            break;
        }
    }

    freeaddrinfo(result);
    if (!connected)
    {
        closesocket(static_cast<SOCKET>(socketFd_));
        socketFd_ = -1;
        return;
    }

    stopReader_ = false;
    const auto connectPacket = buildConnectPacket(qos, timeout);
    if (!writeAll(connectPacket))
    {
        disconnect();
        return;
    }

    readerThread_ = std::thread(&MqttClient::readerLoop, this);

    std::unique_lock<std::mutex> lock(stateMutex_);
    connectedCv_.wait_for(lock, std::chrono::seconds(timeout), [this]() { return isConnected_; });
}

void MqttClient::disconnect()
{
    stopReader_ = true;

    if (socketFd_ >= 0)
    {
        std::vector<std::uint8_t> packet;
        packet.push_back(kDisconnect);
        packet.push_back(0x00);
        writeAll(packet);
        shutdown(static_cast<SOCKET>(socketFd_), SD_BOTH);
        closesocket(static_cast<SOCKET>(socketFd_));
        socketFd_ = -1;
    }

    if (readerThread_.joinable())
    {
        readerThread_.join();
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    isConnected_ = false;
}

void MqttClient::publish(const std::string& topic, const std::string& message)
{
    if (!isConnected_ || socketFd_ < 0)
    {
        return;
    }

    const auto packet = buildPublishPacket(topic, message, qos_);
    writeAll(packet);
}

void MqttClient::subscribe(const std::string& topic)
{
    if (!isConnected_ || socketFd_ < 0)
    {
        return;
    }

    const auto packet = buildSubscribePacket(topic, qos_);
    writeAll(packet);
}

void MqttClient::unsubscribe(const std::string& topic)
{
    if (!isConnected_ || socketFd_ < 0)
    {
        return;
    }

    const auto packet = buildUnsubscribePacket(topic);
    writeAll(packet);
}
