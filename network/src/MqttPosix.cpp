#include "Mqtt.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>

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

bool writeSocket(int socketFd, const std::vector<std::uint8_t>& data)
{
    std::size_t offset = 0;
    while (offset < data.size())
    {
        const auto sent = send(socketFd, data.data() + offset, data.size() - offset, 0);
        if (sent < 0)
        {
            std::cerr << "MqttPosix: send() failed: " << std::strerror(errno) << "\n";
            if (errno == EINTR)
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

bool readExactlyFromSocket(int socketFd, std::vector<std::uint8_t>& buffer, std::size_t expectedSize)
{
    while (buffer.size() < expectedSize)
    {
        std::uint8_t byte = 0;
        const auto received = recv(socketFd, &byte, 1, 0);
        if (received == 1)
        {
            buffer.push_back(byte);
            continue;
        }

        if (received == 0)
        {
            std::cerr << "MqttPosix: recv() returned 0 (connection closed)\n";
            return false;
        }

        if (errno == EINTR)
        {
            continue;
        }

        std::cerr << "MqttPosix: recv() failed: " << std::strerror(errno) << "\n";
        return false;
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
// End of anonymous namespace

MqttClient::MqttClient()
{
    isConnected_ = false;
    socketFd_ = -1;
    qos_ = 1;
    timeout_ = 30;
    keepAliveSeconds_ = std::chrono::seconds(30);
    lastActivity_ = std::chrono::steady_clock::now();
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
    keepAliveSeconds_ = std::chrono::seconds(30);
    lastActivity_ = std::chrono::steady_clock::now();
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
    keepAliveSeconds_ = std::chrono::seconds(30);
    lastActivity_ = std::chrono::steady_clock::now();
    stopReader_ = false;
    nextPacketId_ = 1;
}

MqttClient::~MqttClient()
{
    disconnect();
}

void MqttClient::sendPingRequest()
{
    if (socketFd_ < 0)
    {
        return;
    }

    const std::vector<std::uint8_t> packet = {kPingReq, 0x00};
    if (writeAll(packet))
    {
        lastActivity_ = std::chrono::steady_clock::now();
    }
}

void MqttClient::readerLoop()
{
    while (!stopReader_.load())
    {
        int socketFd = -1;
        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            socketFd = socketFd_;
        }

        if (socketFd < 0)
        {
            break;
        }

        pollfd socketPoll{};
        socketPoll.fd = socketFd;
        socketPoll.events = POLLIN;

        const auto keepAliveWindow = std::chrono::milliseconds(std::max(1000, static_cast<int>(keepAliveSeconds_.count() * 500)));
        const auto now = std::chrono::steady_clock::now();
        if (keepAliveSeconds_.count() > 0 && now - lastActivity_ > keepAliveWindow)
        {
            sendPingRequest();
        }

        const int pollTimeoutMs = static_cast<int>(std::max<long long>(1000LL, keepAliveWindow.count()));
        const int pollResult = poll(&socketPoll, 1, pollTimeoutMs);
        if (pollResult < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (pollResult == 0)
        {
            if (keepAliveSeconds_.count() > 0 && now - lastActivity_ > keepAliveSeconds_)
            {
                break;
            }
            continue;
        }

        if ((socketPoll.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
            break;
        }

        std::vector<std::uint8_t> fixedHeader;
        if (!readExactlyFromSocket(socketFd, fixedHeader, 1))
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
            const auto bytesRead = recv(socketFd, &byte, 1, 0);
            if (bytesRead <= 0)
            {
                if (bytesRead < 0 && errno == EINTR)
                {
                    continue;
                }
                std::cerr << "MqttPosix: failed reading remaining length byte: " << std::strerror(errno) << "\n";
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
            if (!readExactlyFromSocket(socketFd, payload, remainingLength))
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
            lastActivity_ = std::chrono::steady_clock::now();
            if (callback_ != nullptr)
            {
                callback_->onConnected();
            }
            continue;
        }

        if (messageType == kPingResp)
        {
            lastActivity_ = std::chrono::steady_clock::now();
            continue;
        }

        if (messageType == kPublish)
        {
            lastActivity_ = std::chrono::steady_clock::now();
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
    return writeSocket(socketFd_, data);
}

bool MqttClient::readExactly(std::vector<std::uint8_t>& buffer, std::size_t expectedSize)
{
    return readExactlyFromSocket(socketFd_, buffer, expectedSize);
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

std::vector<std::uint8_t> MqttClient::buildPubAckPacket(uint16_t packetId)
{
    std::vector<std::uint8_t> packet;
    packet.push_back(0x40);
    packet.push_back(0x02);
    packet.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
    packet.push_back(static_cast<std::uint8_t>(packetId & 0xFF));
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
    std::uint16_t packetId = 0;
    if (qosFlags == 0x02)
    {
        if (idx + 2 > packet.size())
        {
            return;
        }
        packetId = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[idx]) << 8) | static_cast<std::uint16_t>(packet[idx + 1]));
        idx += 2;
        const auto pubAckPacket = buildPubAckPacket(packetId);
        writeAll(pubAckPacket);
    }

    std::string message(packet.begin() + static_cast<std::ptrdiff_t>(idx), packet.end());
    callback_->onMessageReceived(topic, message);
}

void MqttClient::connect(int qos, int timeout)
{
    qos_ = qos;
    timeout_ = timeout;
    keepAliveSeconds_ = std::chrono::seconds(std::max(1, timeout));
    lastActivity_ = std::chrono::steady_clock::now();

    if (socketFd_ >= 0)
    {
        disconnect();
    }

    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd_ < 0)
    {
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
        close(socketFd_);
        socketFd_ = -1;
        return;
    }

    bool connected = false;
    for (addrinfo* info = result; info != nullptr; info = info->ai_next)
    {
        if (::connect(socketFd_, info->ai_addr, static_cast<socklen_t>(info->ai_addrlen)) == 0)
        {
            connected = true;
            break;
        }
    }

    freeaddrinfo(result);
    if (!connected)
    {
        close(socketFd_);
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

    lastActivity_ = std::chrono::steady_clock::now();
    readerThread_ = std::thread(&MqttClient::readerLoop, this);

    std::unique_lock<std::mutex> lock(stateMutex_);
    connectedCv_.wait_for(lock, std::chrono::seconds(timeout), [this]() { return isConnected_; });
}

void MqttClient::disconnect()
{
    stopReader_ = true;

    int socketFd = -1;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (socketFd_ >= 0)
        {
            socketFd = socketFd_;
            socketFd_ = -1;
        }
    }

    if (socketFd >= 0)
    {
        std::vector<std::uint8_t> packet;
        packet.push_back(kDisconnect);
        packet.push_back(0x00);
        writeSocket(socketFd, packet);
        shutdown(socketFd, SHUT_RDWR);
    }

    if (readerThread_.joinable())
    {
        readerThread_.join();
    }

    if (socketFd >= 0)
    {
        close(socketFd);
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