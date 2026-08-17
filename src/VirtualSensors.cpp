/*
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <ctime>
#include <cstring>
#include "Mqtt.hpp"
#include "Application.hpp"
#include "SignalHandler.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"
#include "Sensing.hpp"

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

class TelemetryCallback : public SensorCallback
{
    void onTelemetryReceived(const Telemetry& telemetry) override
    {
        std::cout << "Telemetry received - Timestamp: " << telemetry.timestamp
                  << ", Value: " << telemetry.value
                  << ", Scale: " << telemetry.scale
                  << ", Unit: " << telemetry.unitofmeasurement
                  << std::endl;
    }
};

namespace
{
unsigned long parseUnsignedLong(const std::string& value)
{
    std::stringstream stream(value);
    unsigned long result = 0;
    stream >> result;
    return result;
}

FlowType parseFlowType(const std::string& value)
{
    if (value == "normal")
    {
        return BaseLine;
    }
    if (value == "spikes")
    {
        return Spikes;
    }
    if (value == "dead")
    {
        return DeadF;
    }
    if (value == "stuck")
    {
        return Stuck;
    }
    if (value == "offset")
    {
        return Offset;
    }
    if (value == "noise")
    {
        return Noise;
    }
    if (value == "drift")
    {
        return Drift;
    }

    return BaseLine;
}

unsigned long parseUnixEpoch(const std::string& value)
{
    std::tm timeInfo = {};
    std::istringstream stream(value);
    stream >> std::get_time(&timeInfo, "%Y/%m/%d %H:%M:%S");
    if (stream.fail())
    {
        return 0;
    }

    return static_cast<unsigned long>(std::mktime(&timeInfo));
}
}

Logger* g_logger = nullptr;
Configuration g_config;
Sensing sensing;
MqttClient client;
SignalHandler signalHandler;

TestMqttCallback callback;
TelemetryCallback scallback;
VirtualSensorSignalHandler sgcallback;

int main(int argc, char* argv[])
{    
    signalHandler.registerCallbackClient(&sgcallback);

    g_logger = Logger::GetInstance();

    g_config.setFileName("VirtualSensors.ini");

    if(g_config.loadConfiguration())
    {
        writeLog("Configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load configuration.", LogLevel::Error);
        return -1;
    }

    std::list<std::string> sections = g_config.getSections();
    for (const std::string& section : sections)
    {
        if (section.compare(0, 5, "FLOW_") != 0)
        {
            continue;
        }

        Flow flow;
        std::memset(&flow, 0, sizeof(flow));

        flow.type = parseFlowType(g_config.getValue(section, "type", "normal"));
        flow.scale = parseUnsignedLong(g_config.getValue(section, "scale", "1"));
        flow.minimum = parseUnsignedLong(g_config.getValue(section, "min", "0"));
        flow.maximum = parseUnsignedLong(g_config.getValue(section, "max", "100"));
        flow.duration = parseUnsignedLong(g_config.getValue(section, "intervalsecs", "300"));
        flow.unixepochstart = parseUnixEpoch(g_config.getValue(section, "start", "1970/01/01 00:00:00"));
        flow.unixepochend = parseUnixEpoch(g_config.getValue(section, "end", "1970/01/01 00:00:00"));

        std::string unit = g_config.getValue(section, "uom", "NA");
        std::strncpy(flow.unitofmeasurement, unit.c_str(), sizeof(flow.unitofmeasurement) - 1);
        flow.unitofmeasurement[sizeof(flow.unitofmeasurement) - 1] = '\0';

        sensing.addFlow(flow);
    }

    // Dump all configuration sections and keys
    std::list<std::string> configSections = g_config.getSections();
    for (const std::string& section : configSections)
    {
        writeLog("Section: " + section, LogLevel::Info);
        std::cout << "Section: " << section << std::endl;

        std::list<std::string> keys = g_config.getKeys(section);
        for (const std::string& key : keys)
        {
            std::string value = g_config.getValue(section, key);

            writeLog("  Key: " + key + ", Value: " + value, LogLevel::Info);
            std::cout << "  Key: " << key << ", Value: " << value << std::endl;
        }
    }

    // Parse the configuration values for MQTT broker address, port, client ID, username, and password
    std::string brokerAddress = g_config.getValue("MQTT", "BrokerAddress", "broker.emqx.io");
    int port = std::stoi(g_config.getValue("MQTT", "Port", "1883"));
    std::string clientId = g_config.getValue("MQTT", "ClientId", "DeviceVirtualClient");
    std::string username = g_config.getValue("MQTT", "Username", "");
    std::string password = g_config.getValue("MQTT", "Password", "");

    client.Initialize(brokerAddress, port, clientId, username, password, &callback);

    std::cout << "Connecting to EMQX broker..." << std::endl;
    client.connect(1, 10);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.subscribe("devicevirtual/test");

    sensing.startSimulation(&scallback);

    std::cout << "Waiting for MQTT activity..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect();
    std::cout << "MQTT test finished." << std::endl;
    return 0;
}