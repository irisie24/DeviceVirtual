#ifndef MQTT_HPP
#define MQTT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Code style -> Strict Allman style
//
// Two classes:
// 1- MqttCallback : pure virtual interface for incoming messages and connection events.
// 2- MqttClient   : MQTT 3.1.1 client (QoS 0 and QoS 1) over a plain TCP socket.
//
// Threading contract
// ------------------
// * connect() and disconnect() must not be called concurrently with each other.
// * publish(), subscribe(), unsubscribe(), isConnected() are thread safe.
// * All MqttCallback methods are invoked on the internal reader thread.
//   - Callbacks may call publish()/subscribe()/unsubscribe()/disconnect().
//   - Callbacks must NOT call connect(). Schedule a reconnect on another thread.
// * onConnectionLost() is only called for unintended loss of an established session,
//   never for a disconnect() requested by the application.
//
// Protocol notes
// --------------
// * QoS 2 is not implemented. A requested QoS of 2 is reduced to 1.
// * Outgoing QoS 1 PUBLISH is sent with a packet id but not retransmitted
//   (clean session, no persistence). Incoming QoS 1 PUBLISH is acknowledged with PUBACK
//   before the callback runs.

#if defined(_WIN32)
using MqttSocket = std::uintptr_t;      // same width as SOCKET
#else
using MqttSocket = int;
#endif
constexpr MqttSocket kInvalidMqttSocket = static_cast<MqttSocket>(-1);

class MqttCallback
{
public:
    virtual ~MqttCallback() = default;
    virtual void onMessageReceived(const std::string& topic, const std::string& message) = 0;
    virtual void onConnectionLost() = 0;
    virtual void onConnected() = 0;
};

class MqttClient
{
    public:
        MqttClient();
        MqttClient(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback);
        ~MqttClient();

        MqttClient(const MqttClient&) = delete;
        MqttClient& operator=(const MqttClient&) = delete;

        void Initialize(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback);

        // qos       : 0 or 1 (default 1).
        // timeout   : seconds allowed for TCP connect plus CONNACK (default 30).
        // keepAlive : MQTT keepalive in seconds, 0 disables it (default 30). PINGREQ is sent
        //             after keepAlive/2 seconds without outgoing traffic, and the session is
        //             dropped if nothing at all is received for keepAlive seconds.
        // Returns true once the broker has accepted the session (CONNACK return code 0).
        bool connect(int qos = 1, int timeout = 30, int keepAlive = 30);

        void disconnect();
        bool isConnected() const;

        // These return true if the packet was handed to the socket, false otherwise.
        bool publish(const std::string& topic, const std::string& message);
        bool subscribe(const std::string& topic);
        bool unsubscribe(const std::string& topic);
    private:
        void readerLoop(MqttSocket sock);
        bool readPacket(MqttSocket sock, std::vector<std::uint8_t>& packet);
        bool readExactly(MqttSocket sock, std::vector<std::uint8_t>& buffer, std::size_t count);
        bool handlePacket(const std::vector<std::uint8_t>& packet, bool& sessionEstablished);
        bool parseIncomingPacket(const std::vector<std::uint8_t>& packet);
        void handleSubAck(const std::vector<std::uint8_t>& packet) const;
        bool sendPingRequest();
        bool writeAll(const std::vector<std::uint8_t>& data);
        std::uint16_t nextPacketId();
        std::vector<std::uint8_t> buildConnectPacket(int qos, int keepAliveSeconds);
        std::vector<std::uint8_t> buildPublishPacket(const std::string& topic, const std::string& message, int qos);
        std::vector<std::uint8_t> buildPubAckPacket(std::uint16_t packetId);
        std::vector<std::uint8_t> buildSubscribePacket(const std::string& topic, int qos);
        std::vector<std::uint8_t> buildUnsubscribePacket(const std::string& topic);

        std::string brokerAddress_;
        int port_ = 1883;
        std::string clientId_;
        std::string username_;
        std::string password_;
        MqttCallback* callback_ = nullptr;

        std::atomic<bool> isConnected_{false};
        std::atomic<MqttSocket> socket_{kInvalidMqttSocket};
        std::atomic<int> qos_{1};
        int timeout_ = 30;
        std::chrono::seconds keepAliveSeconds_{30};

        // steady_clock milliseconds. lastTx_: any packet written. lastRx_: any complete packet read.
        std::atomic<long long> lastTxMs_{0};
        std::atomic<long long> lastRxMs_{0};

        std::thread readerThread_;
        std::mutex socketMutex_;                // serialises writes and guards close()
        std::mutex stateMutex_;                 // guards connectDone_
        std::condition_variable connectedCv_;
        bool connectDone_ = false;              // CONNACK handled (accepted or refused) or reader exited
        std::atomic<bool> stopReader_{false};
        std::atomic<std::uint32_t> nextPacketId_{1};
};

#endif // MQTT_HPP
