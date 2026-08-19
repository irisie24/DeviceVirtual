/*
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <filesystem>
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

unsigned long parseUnsignedLong(const std::string& value);
FlowType parseFlowType(const std::string& value);
unsigned long parseUnixEpoch(const std::string& value);
SimulationMode parseSimulationMode(const std::string& value);
std::list<std::filesystem::path> discoverFlowFiles(const std::string& configDirectory);
bool loadSensorDefinition(const std::filesystem::path& flowFile, Sensing& sensor);

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

        if (loadSensorDefinition(*it, sensor))
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

unsigned long parseUnsignedLong(const std::string& value)
{
    std::stringstream stream(value);
    unsigned long result = 0;
    stream >> result;
    return result;
}

FlowType parseFlowType(const std::string& value)
{
    if (value == "baseline")
    {
        return BaseLine;
    }
    if (value == "normal")
    {
        return Normal;
    }
    if (value == "spikes")
    {
        return Spikes;
    }
    if (value == "dead")
    {
        return Dead;
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
    if (value == "driftlinear")
    {
        return DriftLinear;
    }
    if (value == "driftstepped")
    {
        return DriftStepped;
    }

    return Unknown;
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

SimulationMode parseSimulationMode(const std::string& value)
{
    if (value == "realtime")
    {
        return SimulationModeRealtime;
    }

    return SimulationModeDataset;
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

bool loadSensorDefinition(const std::filesystem::path& flowFile, Sensing& sensor)
{
    Configuration flowConfiguration;

    if (!flowConfiguration.loadCustomConfiguration(flowFile.string()))
    {
        return false;
    }

    if (flowConfiguration.isSection("General"))
    {
        bool repeatFlowSequence = flowConfiguration.getValue("General", "repeat_flow_sequence", "0") == "1";
        unsigned long flowSequenceEnd = parseUnixEpoch(flowConfiguration.getValue("General", "flow_sequence_end", "1970/01/01 00:00:00"));
        SimulationMode simulationMode = parseSimulationMode(flowConfiguration.getValue("General", "simulation_mode", "dataset"));
        sensor.setRepeatable(repeatFlowSequence, flowSequenceEnd);
        sensor.setSimulationMode(simulationMode);
    }

    std::list<std::string> sections = flowConfiguration.getSections();
    for (const std::string& section : sections)
    {
        if (section.compare(0, 5, "FLOW_") != 0)
        {
            continue;
        }

        Flow flow;
        std::memset(&flow, 0, sizeof(flow));

        flow.type = parseFlowType(flowConfiguration.getValue(section, "type", "baseline"));
        flow.scale = parseUnsignedLong(flowConfiguration.getValue(section, "scale", "1"));
        flow.minimum = parseUnsignedLong(flowConfiguration.getValue(section, "min", "0"));
        flow.maximum = parseUnsignedLong(flowConfiguration.getValue(section, "max", "100"));
        flow.duration = parseUnsignedLong(flowConfiguration.getValue(section, "intervalsecs", "300"));
        flow.unixepochstart = parseUnixEpoch(flowConfiguration.getValue(section, "start", "1970/01/01 00:00:00"));
        flow.unixepochend = parseUnixEpoch(flowConfiguration.getValue(section, "end", "1970/01/01 00:00:00"));

        std::string unit = flowConfiguration.getValue(section, "uom", "NA");
        std::strncpy(flow.unitofmeasurement, unit.c_str(), sizeof(flow.unitofmeasurement) - 1);
        flow.unitofmeasurement[sizeof(flow.unitofmeasurement) - 1] = '\0';

        sensor.addFlow(flow);
    }

    return true;
}