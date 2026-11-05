#ifndef MQTT_
#define MQTT_

#include <string>
#include <functional>
#include <algorithm>
#include <cstdbool>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Code style -> Strict Allman style
// Declare two classes.
// 1- Pure virtual MQTT callback class that will be used to handle incoming messages and connection events.
// 2 -Declares a MQTT client class that will be used to connect to a MQTT broker and send/receive messages.

class MqttCallback
{
public:
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

        // Connect will commence with QoS level and timeout parameters. The default QoS level is 1 and the default timeout is 30 seconds.
        void Initialize(const std::string& brokerAddress, const int port, const std::string& clientId, const std::string& username, const std::string& password, MqttCallback* callback);
        void connect(int qos = 1, int timeout = 30);

        void disconnect();
        void publish(const std::string& topic, const std::string& message);
        void subscribe(const std::string& topic);
        void unsubscribe(const std::string& topic);
    private:
        void readerLoop();
        void sendPingRequest();
        bool writeAll(const std::vector<uint8_t>& data);
        bool readExactly(std::vector<uint8_t>& buffer, std::size_t expectedSize);
        std::vector<uint8_t> encodeString(const std::string& value) const;
        std::vector<uint8_t> buildConnectPacket(int qos, int keepAliveSeconds);
        std::vector<uint8_t> buildPublishPacket(const std::string& topic, const std::string& message, int qos);
        std::vector<uint8_t> buildPubAckPacket(uint16_t packetId);
        std::vector<uint8_t> buildSubscribePacket(const std::string& topic, int qos);
        std::vector<uint8_t> buildUnsubscribePacket(const std::string& topic);
        void parseIncomingPacket(const std::vector<uint8_t>& packet);

        std::string brokerAddress_;
        int port_;
        std::string clientId_;
        std::string username_;
        std::string password_;
        bool isConnected_;
        MqttCallback* callback_;
        int socketFd_;
        int qos_;
        int timeout_;
        std::chrono::seconds keepAliveSeconds_;
        std::chrono::steady_clock::time_point lastActivity_;
        std::thread readerThread_;
        std::mutex socketMutex_;
        std::mutex stateMutex_;
        std::condition_variable connectedCv_;
        std::atomic<bool> stopReader_;
        std::atomic<int> nextPacketId_;
};


#endif // MQTT_
