/*
* The entry point of the HTTP REST API server application.
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <thread>
#include <chrono>
#include "Mqtt.hpp"

class TestMqttCallback : public MqttCallback
{
public:
    void onMessageReceived(const std::string& topic, const std::string& message) override
    {
        std::cout << "Received message on topic '" << topic << "': " << message << std::endl;
    }

    void onConnectionLost() override
    {
        std::cout << "MQTT connection lost." << std::endl;
    }

    void onConnected() override
    {
        std::cout << "MQTT connected." << std::endl;
    }
};

int main(int argc, char* argv[])
{
    TestMqttCallback callback;
    MqttClient client("broker.emqx.io", 1883, "DeviceVirtualClient", "", "", &callback);

    std::cout << "Connecting to EMQX broker..." << std::endl;
    client.connect(1, 10);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.subscribe("devicevirtual/test");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.publish("devicevirtual/test", "Hello from DeviceVirtual");

    std::cout << "Waiting for MQTT activity..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect();
    std::cout << "MQTT test finished." << std::endl;
    return 0;
}