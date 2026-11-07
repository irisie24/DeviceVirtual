#include "Mqtt.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
enum class IoStatus
{
    Ok,
    WouldBlock,
    Timeout,
    Closed,
    Error
};

constexpr std::uint8_t kConnect = 0x10;
constexpr std::uint8_t kConnAck = 0x20;
constexpr std::uint8_t kPublish = 0x30;
constexpr std::uint8_t kPubAck = 0x40;
constexpr std::uint8_t kPubRec = 0x50;
constexpr std::uint8_t kPubRel = 0x60;
constexpr std::uint8_t kPubComp = 0x70;
constexpr std::uint8_t kSubscribe = 0x80;
constexpr std::uint8_t kSubAck = 0x90;
constexpr std::uint8_t kUnsubscribe = 0xA0;
constexpr std::uint8_t kUnsubAck = 0xB0;
constexpr std::uint8_t kPingReq = 0xC0;
constexpr std::uint8_t kPingResp = 0xD0;
constexpr std::uint8_t kDisconnect = 0xE0;

constexpr std::uint32_t kMaxRemainingLength = 268435455U;           // MQTT 3.1.1 limit
constexpr std::uint32_t kMaxIncomingRemainingLength = 16U * 1024U * 1024U;
constexpr int kIdleWaitMs = 1000;                                   // reader wakes at least this often
constexpr long long kReadStallMs = 15000;                           // no byte for this long inside a packet
constexpr long long kSendStallMs = 10000;                           // socket not writable for this long

long long nowMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ------------------------------------------------------------------------------------------
// Platform layer (POSIX). The protocol logic below uses only these functions.
// ------------------------------------------------------------------------------------------
const char kTag[] = "MqttPosix";

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;        // never raise SIGPIPE on a closed peer
#else
constexpr int kSendFlags = 0;
#endif

bool platformStartup()
{
    return true;
}

void platformClose(MqttSocket sock)
{
    ::close(sock);
}

void platformShutdown(MqttSocket sock)
{
    ::shutdown(sock, SHUT_RDWR);
}

bool setNonBlocking(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

IoStatus waitFor(MqttSocket sock, short events, int timeoutMs)
{
    pollfd pfd{};
    pfd.fd = sock;
    pfd.events = events;

    const int result = ::poll(&pfd, 1, timeoutMs);
    if (result < 0)
    {
        return (errno == EINTR) ? IoStatus::Timeout : IoStatus::Error;
    }
    if (result == 0)
    {
        return IoStatus::Timeout;
    }
    if ((pfd.revents & POLLNVAL) != 0)
    {
        return IoStatus::Error;
    }

    // POLLIN/POLLOUT, or POLLHUP/POLLERR: the following recv()/send() reports which.
    return IoStatus::Ok;
}

IoStatus platformWaitReadable(MqttSocket sock, int timeoutMs)
{
    return waitFor(sock, POLLIN, timeoutMs);
}

IoStatus platformWaitWritable(MqttSocket sock, int timeoutMs)
{
    return waitFor(sock, POLLOUT, timeoutMs);
}

IoStatus platformRecv(MqttSocket sock, std::uint8_t& byte)
{
    while (true)
    {
        const ssize_t received = ::recv(sock, &byte, 1, 0);
        if (received == 1)
        {
            return IoStatus::Ok;
        }
        if (received == 0)
        {
            return IoStatus::Closed;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return IoStatus::WouldBlock;
        }
        std::cerr << kTag << ": recv() failed: " << std::strerror(errno) << "\n";
        return IoStatus::Error;
    }
}

IoStatus platformSend(MqttSocket sock, const std::uint8_t* data, std::size_t length, std::size_t& sent)
{
    while (true)
    {
        const ssize_t result = ::send(sock, data, length, kSendFlags);
        if (result > 0)
        {
            sent = static_cast<std::size_t>(result);
            return IoStatus::Ok;
        }
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return IoStatus::WouldBlock;
        }
        std::cerr << kTag << ": send() failed: " << std::strerror(errno) << "\n";
        return IoStatus::Error;
    }
}

// Resolves the host, connects with a timeout, and returns a connected NON-BLOCKING socket.
MqttSocket platformConnect(const std::string& host, int port, int timeoutMs)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string portString = std::to_string(port);
    const int rc = ::getaddrinfo(host.c_str(), portString.c_str(), &hints, &result);
    if (rc != 0)
    {
        std::cerr << kTag << ": getaddrinfo() failed: " << ::gai_strerror(rc) << "\n";
        return kInvalidMqttSocket;
    }

    const long long deadlineMs = nowMillis() + timeoutMs;
    MqttSocket connected = kInvalidMqttSocket;

    for (addrinfo* info = result; info != nullptr && connected == kInvalidMqttSocket; info = info->ai_next)
    {
        if (nowMillis() >= deadlineMs)
        {
            break;
        }

        const int fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (fd < 0)
        {
            continue;
        }

        if (!setNonBlocking(fd))
        {
            ::close(fd);
            continue;
        }

        const int rc2 = ::connect(fd, info->ai_addr, static_cast<socklen_t>(info->ai_addrlen));
        if (rc2 != 0 && errno != EINPROGRESS && errno != EINTR)
        {
            ::close(fd);
            continue;
        }

        bool ok = (rc2 == 0);
        while (!ok)
        {
            const long long remainingMs = deadlineMs - nowMillis();
            if (remainingMs <= 0)
            {
                break;
            }

            const IoStatus status = platformWaitWritable(fd, static_cast<int>(std::min<long long>(remainingMs, 1000)));
            if (status == IoStatus::Error)
            {
                break;
            }
            if (status == IoStatus::Ok)
            {
                int soError = 0;
                socklen_t soLen = sizeof(soError);
                ok = (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &soLen) == 0 && soError == 0);
                if (!ok)
                {
                    break;
                }
            }
        }

        if (!ok)
        {
            ::close(fd);
            continue;
        }

        connected = fd;
    }

    ::freeaddrinfo(result);

    if (connected == kInvalidMqttSocket)
    {
        std::cerr << kTag << ": could not connect to " << host << ":" << port << "\n";
        return kInvalidMqttSocket;
    }

    const int one = 1;
    ::setsockopt(connected, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    ::setsockopt(connected, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    return connected;
}

// ------------------------------------------------------------------------------------------
// Protocol helpers
// ------------------------------------------------------------------------------------------
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

std::vector<std::uint8_t> encodeStringPayload(const std::string& value)
{
    std::vector<std::uint8_t> packet;
    const auto length = static_cast<std::uint16_t>(value.size());
    packet.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));
    packet.push_back(static_cast<std::uint8_t>(length & 0xFF));
    packet.insert(packet.end(), value.begin(), value.end());
    return packet;
}

// Writes the whole buffer to a non-blocking socket. A partial write that cannot be
// completed leaves the MQTT stream corrupt, so the caller must treat false as fatal.
bool sendAll(MqttSocket sock, const std::vector<std::uint8_t>& data)
{
    std::size_t offset = 0;
    bool waiting = false;
    long long waitStartMs = 0;

    while (offset < data.size())
    {
        std::size_t sent = 0;
        const IoStatus status = platformSend(sock, data.data() + offset, data.size() - offset, sent);
        if (status == IoStatus::Ok)
        {
            offset += sent;
            waiting = false;
            continue;
        }

        if (status != IoStatus::WouldBlock)
        {
            return false;
        }

        if (!waiting)
        {
            waiting = true;
            waitStartMs = nowMillis();
        }
        else if (nowMillis() - waitStartMs > kSendStallMs)
        {
            std::cerr << kTag << ": send stalled, giving up\n";
            return false;
        }

        if (platformWaitWritable(sock, 200) == IoStatus::Error)
        {
            return false;
        }
    }

    return true;
}

// Index of the first byte after the remaining-length field of an already framed packet.
std::size_t payloadOffset(const std::vector<std::uint8_t>& packet)
{
    std::size_t idx = 1;
    while (idx < packet.size() && (packet[idx++] & 0x80) != 0)
    {
    }
    return idx;
}
} // end of anonymous namespace

// ------------------------------------------------------------------------------------------
// MqttClient
// ------------------------------------------------------------------------------------------
MqttClient::MqttClient()
{
}

MqttClient::MqttClient(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback)
    : brokerAddress_(brokerAddress)
    , port_(port)
    , clientId_(clientId)
    , username_(username)
    , password_(password)
    , callback_(callback)
{
}

MqttClient::~MqttClient()
{
    disconnect();
}

void MqttClient::Initialize(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback)
{
    disconnect();

    brokerAddress_ = brokerAddress;
    port_ = port;
    clientId_ = clientId;
    username_ = username;
    password_ = password;
    callback_ = callback;
}

bool MqttClient::isConnected() const
{
    return isConnected_.load();
}

std::uint16_t MqttClient::nextPacketId()
{
    // MQTT packet identifiers are 1..65535. Zero is invalid and brokers may drop the connection.
    const auto n = nextPacketId_.fetch_add(1, std::memory_order_relaxed);
    return static_cast<std::uint16_t>(n % 65535U + 1U);
}

bool MqttClient::writeAll(const std::vector<std::uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(socketMutex_);

    const MqttSocket sock = socket_.load();
    if (sock == kInvalidMqttSocket)
    {
        return false;
    }

    if (!sendAll(sock, data))
    {
        // Unknown how much reached the broker: the stream is no longer trustworthy.
        // Shutting the socket down makes the reader exit and report the loss.
        platformShutdown(sock);
        return false;
    }

    lastTxMs_.store(nowMillis());
    return true;
}

bool MqttClient::sendPingRequest()
{
    const std::vector<std::uint8_t> packet = {kPingReq, 0x00};
    return writeAll(packet);
}

bool MqttClient::readExactly(MqttSocket sock, std::vector<std::uint8_t>& buffer, std::size_t count)
{
    const std::size_t target = buffer.size() + count;
    bool waiting = false;
    long long waitStartMs = 0;

    while (buffer.size() < target)
    {
        if (stopReader_.load())
        {
            return false;
        }

        // Always one byte at a time, so a read can never run into the next packet.
        std::uint8_t byte = 0;
        const IoStatus status = platformRecv(sock, byte);
        if (status == IoStatus::Ok)
        {
            buffer.push_back(byte);
            waiting = false;
            continue;
        }

        if (status == IoStatus::WouldBlock)
        {
            if (!waiting)
            {
                waiting = true;
                waitStartMs = nowMillis();
            }
            else if (nowMillis() - waitStartMs > kReadStallMs)
            {
                std::cerr << kTag << ": broker stalled in the middle of a packet\n";
                return false;
            }

            if (platformWaitReadable(sock, 1000) == IoStatus::Error)
            {
                return false;
            }
            continue;
        }

        if (status == IoStatus::Closed)
        {
            std::cerr << kTag << ": connection closed by broker\n";
        }
        return false;
    }

    return true;
}

bool MqttClient::readPacket(MqttSocket sock, std::vector<std::uint8_t>& packet)
{
    packet.clear();

    if (!readExactly(sock, packet, 1))
    {
        return false;
    }

    std::uint32_t remainingLength = 0;
    std::uint32_t multiplier = 1;
    bool lengthComplete = false;

    for (int i = 0; i < 4; ++i)
    {
        if (!readExactly(sock, packet, 1))
        {
            return false;
        }

        const std::uint8_t byte = packet.back();
        remainingLength += static_cast<std::uint32_t>(byte & 0x7F) * multiplier;
        if ((byte & 0x80) == 0)
        {
            lengthComplete = true;
            break;
        }
        multiplier *= 128;
    }

    if (!lengthComplete)
    {
        std::cerr << kTag << ": malformed remaining length, closing\n";
        return false;
    }

    if (remainingLength > kMaxIncomingRemainingLength)
    {
        std::cerr << kTag << ": incoming packet too large (" << remainingLength << " bytes), closing\n";
        return false;
    }

    return (remainingLength == 0) || readExactly(sock, packet, remainingLength);
}

void MqttClient::readerLoop(MqttSocket sock)
{
    bool sessionEstablished = false;
    const long long keepAliveMs = static_cast<long long>(keepAliveSeconds_.count()) * 1000LL;
    const long long halfMs = keepAliveMs / 2;

    while (!stopReader_.load())
    {
        int waitMs = kIdleWaitMs;

        if (sessionEstablished && keepAliveMs > 0)
        {
            long long now = nowMillis();

            // Pings go out every keepAlive/2, so the broker answers at least that often.
            // Silence for a whole keepAlive interval means the peer or the path is dead.
            if (now - lastRxMs_.load() > keepAliveMs)
            {
                std::cerr << kTag << ": nothing received from broker for " << keepAliveSeconds_.count() << " s, closing\n";
                break;
            }

            if (now - lastTxMs_.load() >= halfMs)
            {
                if (!sendPingRequest())
                {
                    break;
                }
                now = nowMillis();
            }

            const long long untilPingMs = halfMs - (now - lastTxMs_.load());
            waitMs = static_cast<int>(std::max<long long>(20, std::min<long long>(untilPingMs, kIdleWaitMs)));
        }

        const IoStatus ready = platformWaitReadable(sock, waitMs);
        if (ready == IoStatus::Timeout)
        {
            continue;
        }
        if (ready == IoStatus::Error)
        {
            break;
        }

        std::vector<std::uint8_t> packet;
        if (!readPacket(sock, packet))
        {
            break;
        }

        lastRxMs_.store(nowMillis());

        if (!handlePacket(packet, sessionEstablished))
        {
            break;
        }
    }

    // Wake any writer and release the peer; the descriptor itself is closed by disconnect().
    platformShutdown(sock);
    isConnected_.store(false);

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        connectDone_ = true;
    }
    connectedCv_.notify_all();

    if (sessionEstablished && !stopReader_.load() && callback_ != nullptr)
    {
        callback_->onConnectionLost();
    }
}

bool MqttClient::handlePacket(const std::vector<std::uint8_t>& packet, bool& sessionEstablished)
{
    const std::uint8_t messageType = packet[0] & 0xF0;

    if (!sessionEstablished && messageType != kConnAck)
    {
        std::cerr << kTag << ": packet type 0x" << std::hex << static_cast<int>(messageType) << std::dec << " before CONNACK, closing\n";
        return false;
    }

    switch (messageType)
    {
    case kConnAck:
    {
        if (sessionEstablished)
        {
            std::cerr << kTag << ": duplicate CONNACK, closing\n";
            return false;
        }

        // Fixed header (2) + session-present flags (1) + return code (1).
        const bool accepted = (packet.size() >= 4) && (packet[3] == 0x00);
        if (!accepted)
        {
            std::cerr << kTag << ": CONNACK refused, return code " << (packet.size() >= 4 ? static_cast<int>(packet[3]) : -1) << "\n";
        }

        sessionEstablished = accepted;
        isConnected_.store(accepted);
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            connectDone_ = true;
        }
        connectedCv_.notify_all();

        if (!accepted)
        {
            return false;
        }

        // Called without any lock held, so the callback may call back into the client.
        if (callback_ != nullptr)
        {
            callback_->onConnected();
        }
        return true;
    }

    case kPublish:
        return parseIncomingPacket(packet);

    case kSubAck:
        handleSubAck(packet);
        return true;

    case kPubRec:
    case kPubRel:
    case kPubComp:
        // Only reachable if the broker ignores the QoS we asked for.
        std::cerr << kTag << ": QoS 2 packet received but QoS 2 is not implemented, closing\n";
        return false;

    case kPingResp:
    case kPubAck:
    case kUnsubAck:
    default:
        return true;    // nothing to do; lastRx_ already refreshed
    }
}

void MqttClient::handleSubAck(const std::vector<std::uint8_t>& packet) const
{
    // payload: packet id (2) then one return code per topic filter; 0x80 means refused.
    for (std::size_t i = payloadOffset(packet) + 2; i < packet.size(); ++i)
    {
        if (packet[i] == 0x80)
        {
            std::cerr << kTag << ": SUBSCRIBE refused by broker\n";
        }
    }
}

bool MqttClient::parseIncomingPacket(const std::vector<std::uint8_t>& packet)
{
    const std::size_t size = packet.size();
    const int qos = (packet[0] >> 1) & 0x03;

    if (qos == 2)
    {
        std::cerr << kTag << ": QoS 2 PUBLISH received but QoS 2 is not implemented, closing\n";
        return false;
    }
    if (qos == 3)
    {
        std::cerr << kTag << ": malformed PUBLISH flags, closing\n";
        return false;
    }

    std::size_t idx = payloadOffset(packet);

    if (idx + 2 > size)
    {
        std::cerr << kTag << ": truncated PUBLISH, closing\n";
        return false;
    }

    const std::size_t topicLength = (static_cast<std::size_t>(packet[idx]) << 8) | static_cast<std::size_t>(packet[idx + 1]);
    idx += 2;

    if (idx + topicLength > size)
    {
        std::cerr << kTag << ": truncated PUBLISH topic, closing\n";
        return false;
    }

    const std::string topic(packet.begin() + static_cast<std::ptrdiff_t>(idx), packet.begin() + static_cast<std::ptrdiff_t>(idx + topicLength));
    idx += topicLength;

    std::uint16_t packetId = 0;
    if (qos == 1)
    {
        if (idx + 2 > size)
        {
            std::cerr << kTag << ": truncated PUBLISH packet id, closing\n";
            return false;
        }
        packetId = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[idx]) << 8) | static_cast<std::uint16_t>(packet[idx + 1]));
        idx += 2;

        // Acknowledge first: the broker's in-flight window is released even if the callback is slow.
        if (!writeAll(buildPubAckPacket(packetId)))
        {
            return false;
        }
    }

    if (callback_ != nullptr)
    {
        const std::string message(packet.begin() + static_cast<std::ptrdiff_t>(idx), packet.end());
        callback_->onMessageReceived(topic, message);
    }

    return true;
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

    // Clean session. MQTT 3.1.1: the password flag requires the user name flag.
    std::uint8_t connectFlags = 0x02;
    if (!username_.empty())
    {
        connectFlags |= 0x80;
        if (!password_.empty())
        {
            connectFlags |= 0x40;
        }
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

        if (!password_.empty())
        {
            const auto password = encodeStringPayload(password_);
            payload.insert(payload.end(), password.begin(), password.end());
        }
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
        const std::uint16_t packetId = nextPacketId();
        payload.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
        payload.push_back(static_cast<std::uint8_t>(packetId & 0xFF));
    }

    payload.insert(payload.end(), message.begin(), message.end());
    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<std::uint8_t> MqttClient::buildPubAckPacket(std::uint16_t packetId)
{
    std::vector<std::uint8_t> packet;
    packet.push_back(kPubAck);
    packet.push_back(0x02);
    packet.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
    packet.push_back(static_cast<std::uint8_t>(packetId & 0xFF));
    return packet;
}

std::vector<std::uint8_t> MqttClient::buildSubscribePacket(const std::string& topic, int qos)
{
    std::vector<std::uint8_t> packet;
    packet.push_back(kSubscribe | 0x02);

    const std::uint16_t packetId = nextPacketId();
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

    const std::uint16_t packetId = nextPacketId();
    std::vector<std::uint8_t> payload;
    payload.push_back(static_cast<std::uint8_t>((packetId >> 8) & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(packetId & 0xFF));

    const auto topicPayload = encodeStringPayload(topic);
    payload.insert(payload.end(), topicPayload.begin(), topicPayload.end());

    encodeRemainingLength(packet, static_cast<std::uint32_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

bool MqttClient::connect(int qos, int timeout, int keepAlive)
{
    if (readerThread_.joinable() && readerThread_.get_id() == std::this_thread::get_id())
    {
        std::cerr << kTag << ": connect() must not be called from a callback\n";
        return false;
    }

    if (!platformStartup())
    {
        std::cerr << kTag << ": socket subsystem start-up failed\n";
        return false;
    }

    if (clientId_.size() > 65535 || username_.size() > 65535 || password_.size() > 65535)
    {
        std::cerr << kTag << ": client id, user name or password too long\n";
        return false;
    }

    // Tear down any previous session; this joins the reader thread.
    disconnect();

    if (qos > 1)
    {
        std::cerr << kTag << ": QoS 2 is not implemented, using QoS 1\n";
    }
    qos_.store(qos > 0 ? 1 : 0);
    timeout_ = std::min(std::max(1, timeout), 3600);
    keepAliveSeconds_ = std::chrono::seconds(std::min(std::max(0, keepAlive), 65535));

    stopReader_.store(false);
    isConnected_.store(false);
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        connectDone_ = false;
    }

    const long long deadlineMs = nowMillis() + static_cast<long long>(timeout_) * 1000LL;

    const MqttSocket sock = platformConnect(brokerAddress_, port_, timeout_ * 1000);
    if (sock == kInvalidMqttSocket)
    {
        return false;
    }

    socket_.store(sock);
    lastTxMs_.store(nowMillis());
    lastRxMs_.store(nowMillis());

    if (!writeAll(buildConnectPacket(qos_.load(), static_cast<int>(keepAliveSeconds_.count()))))
    {
        disconnect();
        return false;
    }

    readerThread_ = std::thread(&MqttClient::readerLoop, this, sock);

    {
        std::unique_lock<std::mutex> lock(stateMutex_);
        const long long remainingMs = std::max<long long>(1, deadlineMs - nowMillis());
        connectedCv_.wait_for(lock, std::chrono::milliseconds(remainingMs), [this]() { return connectDone_; });
    }

    if (!isConnected_.load())
    {
        std::cerr << kTag << ": no CONNACK accepted within " << timeout_ << " s\n";
        disconnect();
        return false;
    }

    return true;
}

void MqttClient::disconnect()
{
    stopReader_.store(true);

    if (readerThread_.joinable() && readerThread_.get_id() == std::this_thread::get_id())
    {
        // Called from a callback on the reader thread, which cannot join itself.
        // Ask the reader to leave; the descriptor is released by the next disconnect() call
        // from another thread (connect() and the destructor both make one).
        isConnected_.store(false);
        const MqttSocket current = socket_.load();
        if (current != kInvalidMqttSocket)
        {
            platformShutdown(current);
        }
        return;
    }

    const MqttSocket sock = socket_.exchange(kInvalidMqttSocket);
    if (sock != kInvalidMqttSocket)
    {
        {
            // Best-effort DISCONNECT on a live session. If a writer is stuck holding the
            // socket, skip it: the shutdown below unblocks that writer.
            std::unique_lock<std::mutex> lock(socketMutex_, std::try_to_lock);
            if (lock.owns_lock() && isConnected_.load())
            {
                const std::vector<std::uint8_t> packet = {kDisconnect, 0x00};
                sendAll(sock, packet);
            }
        }
        platformShutdown(sock);
    }

    if (readerThread_.joinable())
    {
        readerThread_.join();
    }

    if (sock != kInvalidMqttSocket)
    {
        // Close under the write mutex: no writer can be mid-send on this descriptor.
        std::lock_guard<std::mutex> lock(socketMutex_);
        platformClose(sock);
    }

    isConnected_.store(false);
}

bool MqttClient::publish(const std::string& topic, const std::string& message)
{
    if (!isConnected_.load())
    {
        return false;
    }

    if (topic.empty() || topic.size() > 65535 || topic.size() + message.size() + 4 > kMaxRemainingLength)
    {
        return false;
    }

    return writeAll(buildPublishPacket(topic, message, qos_.load()));
}

bool MqttClient::subscribe(const std::string& topic)
{
    if (!isConnected_.load() || topic.empty() || topic.size() > 65535)
    {
        return false;
    }

    // qos_ is already limited to 0 or 1: the QoS 2 handshake is not implemented.
    return writeAll(buildSubscribePacket(topic, qos_.load()));
}

bool MqttClient::unsubscribe(const std::string& topic)
{
    if (!isConnected_.load() || topic.empty() || topic.size() > 65535)
    {
        return false;
    }

    return writeAll(buildUnsubscribePacket(topic));
}
