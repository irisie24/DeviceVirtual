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
#include "Device.hpp"

class DeviceVirtualMqttCallback : public MqttCallback
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

class DeviceVirtualSignalHandler : public SignalCallback
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
			sensingClient.disconnect();
			exit(0);
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

    void onTelemetryReceived(const std::string& deviceid, const Telemetry& telemetry, SimulationMode mode) override
    {
        std::cout << "Telemetry received - Device ID: " << deviceid
                  << ", Timestamp: " << telemetry.timestamp
                  << ", Value: " << telemetry.value
                  << ", Scale: " << telemetry.scale
                  << ", Unit: " << telemetry.unitofmeasurement
                  << std::endl;

		if (mode == SimulationModeDataset)
		{
			return;
		}

        if (_client != nullptr && !_publishTopic.empty())
        {
            std::ostringstream message;
            message << "deviceid=" << deviceid << ",timestamp=" << telemetry.timestamp
                    << ",value=" << telemetry.value
                    << ",scale=" << telemetry.scale
                    << ",unit=" << telemetry.unitofmeasurement;
            _client->publish(_publishTopic + deviceid, message.str());
        }
    }

    void onTelemetryGenerationComplete(const std::string& deviceid, const std::string& telemetryfilename) override
    {
        writeLogNormal("Telemetry generation complete - Device ID: " + deviceid + ", File: " + telemetryfilename);
    }

    void onMessage(const std::string& deviceid, const std::string& msg) override
    {
        writeLogNormal("Device ID: " + deviceid + ", Message: " + msg);
    }

private:
    MqttClient* _client;
    std::string _publishTopic;
};

std::list<std::filesystem::path> discoverFlowFiles(const std::string& configDirectory);

// These are local to this files as the class definition of the callbacks are only used here
DeviceVirtualMqttCallback callback;
TelemetryCallback scallback;
DeviceVirtualSignalHandler signalEvents;

// The messaging flows is as follows:
// Sensing class fires TelemetryCallback -> Published to broker -> DeviceVirtualMqttCallback receives it -> Actuation class acts upon it -> Actuation class publishes to broker any action taken

int main(int argc, char* argv[])
{    
    loggerRefPtr = Logger::GetInstance();
	loggerRefPtr->startLogging(LogFileMode::Append);

    signalHandler.registerCallbackClient(&signalEvents);

    // Load application configuration
    appConfig.setConfigDirectory(resolveDataPath());
    appConfig.setConfigFileName("Application.ini");
    if(appConfig.loadConfiguration())
    {
        writeLog("Application configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load application configuration.", LogLevel::Error);
        writeLog(std::string(appConfig.getConfigDirectory()+"/" + appConfig.getConfigFileName()).c_str(), LogLevel::Error);
        return -1;
    }

    // Load device configuration
    devConfig.setConfigDirectory(resolveDataPath());
    devConfig.setConfigFileName("Device.ini");
    if(devConfig.loadConfiguration())
    {
        writeLog("Device configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load device configuration.", LogLevel::Error);
        writeLog(std::string(devConfig.getConfigDirectory()+"/" + devConfig.getConfigFileName()).c_str(), LogLevel::Error);
        return -1;
    }

    // Load sensing configuration
    snsConfig.setConfigDirectory(resolveDataPath());
    snsConfig.setConfigFileName("Sensing.ini");
    if(snsConfig.loadConfiguration())
    {
        writeLog("Sensing configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load sensing configuration.", LogLevel::Error);
        writeLog(std::string(snsConfig.getConfigDirectory()+"/" + snsConfig.getConfigFileName()).c_str(), LogLevel::Error);
        return -1;
    }

    // Load actuation configuration
    actConfig.setConfigDirectory(resolveDataPath());
    actConfig.setConfigFileName("Actuation.ini");
    if(actConfig.loadConfiguration())
    {
        writeLog("Actuation configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load actuation configuration.", LogLevel::Error);
        writeLog(std::string(actConfig.getConfigDirectory()+"/" + actConfig.getConfigFileName()).c_str(), LogLevel::Error);
        return -1;
    }

    // 1 We will process the application settings 
    // Parse the configuration values for MQTT broker address, port, client ID, username, and password
    brokerAddress = appConfig.getValue("MQTT", "BrokerAddress", "127.0.0.1");
    port = std::stoi(appConfig.getValue("MQTT", "Port", "1883"));
    clientId = appConfig.getValue("MQTT", "ClientId", "DeviceVirtual");
    username = appConfig.getValue("MQTT", "Username", "");
    password = appConfig.getValue("MQTT", "Password", "");
    mqttPublishTopic = appConfig.getValue("MQTT", "publishtopic", "devicevirtual/sensing/");
    mqttSubscribeTopic = appConfig.getValue("MQTT", "subscribetopic", "devicevirtual/actuation/");

    // 2 We will process the device settings
    // Parse the device settings from the configuration
    device.loadDeviceDefinition(devConfig);

    // 3 Load sensing flow definitions for the device
    // The device class internally uses the Sensing class to manage flow definitions
    device.loadSensingFlowDefinition(snsConfig);

    // 4 Load actuation rule definitions for the device
    device.loadActuationRuleDefinition(actConfig);

    // Intialize the sensing mqtt client
    sensingClient.Initialize(brokerAddress, port, clientId, username, password, &callback);
    sensingClient.connect(1, 3600);

    // Intialize the actuation mqtt client
    actuationClient.Initialize(brokerAddress, port, clientId, username, password, &callback);
    actuationClient.connect(1, 3600);

    /*
    appConfig.setConfigFileName("VirtualSensors.ini");

    if(appConfig.loadConfiguration())
    {
        writeLog("Configuration loaded successfully.", LogLevel::Info);
    }
    else
    {
        writeLog("Failed to load configuration.", LogLevel::Error);
        writeLog(appConfig.getConfigDirectory().c_str(), LogLevel::Error);
        writeLog("VirtualSensors.ini", LogLevel::Error);
        return -1;
    }

    std::string configDirectory = appConfig.getConfigDirectory();
    std::filesystem::path deviceDefinitionPath = std::filesystem::path(configDirectory) / "Device.ini";
    Device deviceTemplate;
    if (!deviceTemplate.loadDeviceDefinition(deviceDefinitionPath))
    {
        writeLog("Failed to load device definition: " + deviceDefinitionPath.string(), LogLevel::Error);
        return -1;
    }

    std::list<Device> listOfDevices;
    std::list<std::filesystem::path> flowFiles = discoverFlowFiles(configDirectory);

    for (std::list<std::filesystem::path>::const_iterator it = flowFiles.begin(); it != flowFiles.end(); ++it)
    {
        Device device = deviceTemplate;

        if (device.loadSensingFlowDefinition(*it))
        {
            // Set the flow set name based on the filename (without extension)
            std::string flowSetName = it->stem().string();
            device.getSensing().setFlowSetName(flowSetName);
            listOfDevices.push_back(device);
            writeLog("Loaded flow definition: " + it->filename().string(), LogLevel::Info);
        }
        else
        {
            writeLog("Failed to load flow definition: " + it->string(), LogLevel::Error);
        }
    }

    if (listOfDevices.empty())
    {
        writeLog("No flow definitions were loaded.", LogLevel::Error);
        return -1;
    }

    bool hasRealtimeSimulation = false;
    for (std::list<Device>::iterator it = listOfDevices.begin(); it != listOfDevices.end(); ++it)
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
    std::string mqttPublishTopic = appConfig.getValue("MQTT", "publishtopic", "devicevirtual/sensing/");
    std::string mqttSubscribeTopic = appConfig.getValue("MQTT", "subscribetopic", "devicevirtual/actuation/");

    client.Initialize(brokerAddress, port, clientId, username, password, &callback);
    scallback.setClient(&client);
    scallback.setPublishTopic(mqttPublishTopic);

    std::cout << "Connecting to broker..." << std::endl;
    client.connect(1, 10);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!mqttSubscribeTopic.empty())
    {
        client.subscribe(mqttSubscribeTopic);
    }

    for (std::list<Device>::iterator it = listOfDevices.begin(); it != listOfDevices.end(); ++it)
    {
        it->startSensingSimulation(&scallback);
    }

	std::cout << "Press Ctrl+C to exit the application." << std::endl;

    // Keep the main thread alive until the user requests shutdown (Ctrl+C).
    // Previous implementation slept for a fixed 10 seconds which could let
    // the program exit while background threads (including the MQTT reader)
    // were still active, causing socket closes and WSAENOTSOCK/WSAEINTR.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect();
    std::cout << "MQTT test finished." << std::endl;
    */

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
