/*
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sstream>
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
            writeLog("Application suspended.", LogLevel::Info);
        }

        void resume() override
        {
            writeLog("Application resumed.", LogLevel::Info);
        }

        void shutdown() override
        {
            writeLog("Application shutting down.", LogLevel::Info);
        }

        void alarm() override
        {
            writeLog("Alarm signal received.", LogLevel::Info);
        }

        void reset() override
        {
            writeLog("Reset signal received.", LogLevel::Info);
        }

        void childExit() override
        {
            writeLog("Child process exited.", LogLevel::Info);
        }

        void userdefined1() override
        {
            writeLog("User-defined signal 1 received.", LogLevel::Info);
        }

        void userdefined2() override
        {
            writeLog("User-defined signal 2 received.", LogLevel::Info);
        }
};

class TelemetryCallback : public SensorCallback
{
public:
    TelemetryCallback()
        : _client(nullptr)
    {
    }

    void setClient(MqttClient* client)
    {
        _client = client;
    }

    void setPublishTopic(const std::string& publishTopic)
    {
        _publishTopic = publishTopic;
    }

    void onTelemetryReceived(const Telemetry& telemetry) override
    {
        std::cout << "Telemetry received - Timestamp: " << telemetry.timestamp
                  << ", Value: " << telemetry.value
                  << ", Scale: " << telemetry.scale
                  << ", Unit: " << telemetry.unitofmeasurement
                  << std::endl;

        if (_client != nullptr && !_publishTopic.empty())
        {
            std::ostringstream message;
            message << "timestamp=" << telemetry.timestamp
                    << ",value=" << telemetry.value
                    << ",scale=" << telemetry.scale
                    << ",unit=" << telemetry.unitofmeasurement;
            _client->publish(_publishTopic, message.str());
        }
    }

    void onTelemetryGenerationComplete(const std::string& telemetryfilename) override
    {
        std::cout << "Telemetry generation complete - File: " << telemetryfilename << std::endl;
    }

private:
    MqttClient* _client;
    std::string _publishTopic;
};

std::list<std::filesystem::path> discoverFlowFiles(const std::string& configDirectory);

Configuration appConfig;
MqttClient client;
SignalHandler signalHandler;

TestMqttCallback callback;
TelemetryCallback scallback;
VirtualSensorSignalHandler sgcallback;

int main(int argc, char* argv[])
{    
    signalHandler.registerCallbackClient(&sgcallback);

    appConfig.setFileName("VirtualSensors.ini");

    if(appConfig.loadConfiguration())
    {
        writeLog("Configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load configuration.", LogLevel::Error);
        return -1;
    }

    std::list<Sensing> listofSensors;
    std::string configDirectory = appConfig.getConfigDirectory();
    std::list<std::filesystem::path> flowFiles = discoverFlowFiles(configDirectory);

    for (std::list<std::filesystem::path>::const_iterator it = flowFiles.begin(); it != flowFiles.end(); ++it)
    {
        Sensing sensor;

        if (sensor.loadSensorDefinition(*it))
        {
            // Set the flow set name based on the filename (without extension)
            std::string flowSetName = it->stem().string();
            sensor.setFlowSetName(flowSetName);
            listofSensors.push_back(sensor);
            writeLog("Loaded flow definition: " + it->filename().string(), LogLevel::Info);
        }
        else
        {
            writeLog("Failed to load flow definition: " + it->string(), LogLevel::Error);
        }
    }

    if (listofSensors.empty())
    {
        writeLog("No flow definitions were loaded.", LogLevel::Error);
        return -1;
    }

    bool hasRealtimeSimulation = false;
    for (std::list<Sensing>::iterator it = listofSensors.begin(); it != listofSensors.end(); ++it)
    {
        if (it->getSimulationMode() == SimulationModeRealtime)
        {
            hasRealtimeSimulation = true;
            break;
        }
    }


    // Parse the configuration values for MQTT broker address, port, client ID, username, and password
    std::string brokerAddress = appConfig.getValue("MQTT", "BrokerAddress", "broker.emqx.io");
    int port = std::stoi(appConfig.getValue("MQTT", "Port", "1883"));
    std::string clientId = appConfig.getValue("MQTT", "ClientId", "DeviceVirtualClient");
    std::string username = appConfig.getValue("MQTT", "Username", "");
    std::string password = appConfig.getValue("MQTT", "Password", "");
    std::string mqttPublishTopic = appConfig.getValue("MQTT", "publishtopic", "devicevirtual/telemetry");
    std::string mqttSubscribeTopic = appConfig.getValue("MQTT", "subscribetopic", "devicevirtual/test");

    client.Initialize(brokerAddress, port, clientId, username, password, &callback);
    scallback.setClient(&client);
    scallback.setPublishTopic(mqttPublishTopic);

    std::cout << "Connecting to EMQX broker..." << std::endl;
    client.connect(1, 10);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!mqttSubscribeTopic.empty())
    {
        client.subscribe(mqttSubscribeTopic);
    }

    for (std::list<Sensing>::iterator it = listofSensors.begin(); it != listofSensors.end(); ++it)
    {
        it->startSimulation(&scallback);
    }

    if (hasRealtimeSimulation)
    {
        std::cout << "Waiting for MQTT activity..." << std::endl;
        for (int i = 0; i < 10; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else
    {
        std::cout << "MQTT activity completed in dataset mode." << std::endl;
    }

    client.disconnect();
    std::cout << "MQTT test finished." << std::endl;
    return 0;
}

std::list<std::filesystem::path> discoverFlowFiles(const std::string& configDirectory)
{
    std::list<std::filesystem::path> flowFiles;

    if (configDirectory.empty())
    {
        return flowFiles;
    }

    std::filesystem::path directoryPath(configDirectory);
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
    {
        return flowFiles;
    }

    for (std::filesystem::directory_iterator it(directoryPath); it != std::filesystem::directory_iterator(); ++it)
    {
        if (!it->is_regular_file())
        {
            continue;
        }

        if (it->path().extension() == ".flow")
        {
            flowFiles.push_back(it->path());
        }
    }

    flowFiles.sort();
    return flowFiles;
}

