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

// These are local to this files as the class definition of the callbacks are only used here

class DeviceVirtualActuationCallback : public ActuationCallback
{
public:
    void onActionTriggered(const ActuationDecision& decision) override
    {
        std::stringstream str;
        str << "Actuation triggered - Rule ID: " << decision.ruleId
            << ", Action Type: " << static_cast<int>(decision.actionType)
            << ", IO: " << decision.actionIo
            << ", Value: " << decision.actionValue;
        writeLog(str.str(), LogLevel::Info);

        // The topic we want to publish the actuation result to would typically be derived from the decision or device ID
        std::string actuationTopic = topic + "/sensing/"+ device.getDeviceId();
        actuationClient.publish(actuationTopic, str.str());
        writeLog("Publishing actuation result to topic: " + actuationTopic, LogLevel::Info);
    }

    void onRuleEvaluationComplete(long sensorValue) override
    {
        std::stringstream str;
        str << "Actuation evaluation complete for sensor value: " << sensorValue;
        writeLog(str.str(), LogLevel::Info);
    }
};

DeviceVirtualActuationCallback actuationDecisionCallback;

class DeviceVirtualMqttCallback : public MqttCallback
{
public:
    void onMessageReceived(const std::string& topic, const std::string& message) override
    {
        std::string str = "Received message on topic '" + topic + "': " + message;
        writeLog(str, LogLevel::Debug);

        const std::string deviceIdFromTopic = parseDeviceIdFromTopic(topic);
        const std::string deviceIdFromPayload = getTelemetryFieldValue(message, "deviceid");
        std::string targetDeviceId = deviceIdFromTopic;
        if (!deviceIdFromPayload.empty())
        {
            targetDeviceId = deviceIdFromPayload;
        }

        if (targetDeviceId.empty())
        {
            writeLog("Ignoring message because device id was not found in topic or payload.", LogLevel::Debug);
            return;
        }

        const std::string currentDeviceId = device.getDeviceId();
        if (!currentDeviceId.empty() && currentDeviceId != targetDeviceId)
        {
            writeLog("Ignoring message for a different device id: " + targetDeviceId, LogLevel::Debug);
            return;
        }

        const std::string valueText = getTelemetryFieldValue(message, "value");
        long sensorValue = 0;
        if (valueText.empty() || !parseLongValue(valueText, sensorValue))
        {
            writeLog("Ignoring message because sensor value is missing or invalid.", LogLevel::Debug);
            return;
        }

        const bool matched = device.getActuation().evaluate(sensorValue, &actuationDecisionCallback);
        if (!matched)
        {
            writeLog("No actuation rule matched for sensor value: " + std::to_string(sensorValue), LogLevel::Debug);
        }
    }

    void onConnectionLost() override
    {
        writeLog("MQTT connection lost.", LogLevel::Error);
    }

    void onConnected() override
    {
        writeLog("MQTT connected.", LogLevel::Info);
    }
};

DeviceVirtualMqttCallback callback;

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
            running = false;
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

DeviceVirtualSignalHandler signalEvents;

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
        std::stringstream str;
        str << "Telemetry received - Device ID: " << deviceid
                  << ", Timestamp: " << telemetry.timestamp
                  << ", Value: " << telemetry.value
                  << ", Scale: " << telemetry.scale
                  << ", Unit: " << telemetry.unitofmeasurement;

        writeLog(str.str(), LogLevel::Info);

		if (mode == SimulationModeDataset)
		{
            writeLog("Telemetry received in dataset mode - Device ID: " + deviceid, LogLevel::Info);
			return;
		}

        if (_client != nullptr && !_publishTopic.empty())
        {
            writeLog("Publishing telemetry to broker - Device ID: " + deviceid, LogLevel::Info);
            std::ostringstream message;
            message << "deviceid=" << deviceid << ",timestamp=" << telemetry.timestamp
                    << ",value=" << telemetry.value
                    << ",scale=" << telemetry.scale
                    << ",unit=" << telemetry.unitofmeasurement;
            _client->publish(_publishTopic + "/sensing/"+ deviceid, message.str());
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

TelemetryCallback telemetryCallback;

// The messaging flows is as follows:
// Sensing class fires TelemetryCallback -> Published to broker -> DeviceVirtualMqttCallback receives it -> Actuation class acts upon it -> Actuation class publishes to broker any action taken

int main(int argc, char* argv[])
{    
    loggerRefPtr = Logger::GetInstance();
	loggerRefPtr->startLogging(LogFileMode::CreateNew);
    writeLog("Starting Device Virtual application.", LogLevel::Info);

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
    snsConfig.setConfigFileName("Sensing.flow.ini");
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
    actConfig.setConfigFileName("Actuation.rule.ini");
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
    topic = appConfig.getValue("MQTT", "topic", "devicevirtual");

    // 2 We will process the device settings
    // Parse the device settings from the configuration
    device.loadDeviceDefinition(devConfig);

    // 3 Load sensing flow definitions for the device
    // The device class internally uses the Sensing class to manage flow definitions
    device.loadSensingFlowDefinition(snsConfig);

    // 4 Load actuation rule definitions for the device
    device.loadActuationRuleDefinition(actConfig);

    // Intialize the sensing mqtt client
    sensingClient.Initialize(brokerAddress, port, clientId+"Sensing", username, password, &callback);
    sensingClient.connect(1, 3600);

    // Intialize the actuation mqtt client
    actuationClient.Initialize(brokerAddress, port, clientId+"Actuation", username, password, &callback);
    actuationClient.connect(1, 3600);

    telemetryCallback.setClient(&sensingClient);
    telemetryCallback.setPublishTopic(topic);
    device.startSensingSimulation(&telemetryCallback);

    // We will catch the same messages published to the topic for actuation
    actuationClient.subscribe(topic+"/sensing/#");

    while(running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
 
    sensingClient.disconnect();
    actuationClient.disconnect();
    writeLog("MQTT clients disconnected.", LogLevel::Info);

    return 0;
}
