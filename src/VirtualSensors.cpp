/*
* The entry point of the HTTP REST API server application.
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <thread>
#include <chrono>
#include "Mqtt.hpp"
#include "Application.hpp"
#include "SignalHandler.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"

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

class VirtualSensorSignalHandler : public SignalCallback
{
    public:
        void suspend() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Application suspended.");
        }

        void resume() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Application resumed.");
        }

        void shutdown() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Application shutting down.");
        }

        void alarm() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Alarm signal received.");
        }

        void reset() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Reset signal received.");
        }

        void childExit() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "Child process exited.");
        }

        void userdefined1() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "User-defined signal 1 received.");
        }

        void userdefined2() override
        {
            Logger::GetInstance()->writeExtended(LogLevel::Info, __func__, __FILE__, __LINE__, "User-defined signal 2 received.");
        }
};

Logger* g_logger = nullptr;
Configuration g_config;

int main(int argc, char* argv[])
{    
    g_logger = Logger::GetInstance();

    if(g_config.loadCustomConfiguration("DeviceVirtual.ini"))
    {
        writeLog("Configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load configuration.", LogLevel::Error);
        return -1;
    }

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