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
        std::string actuationTopic = topic + "/actuation/" + device.getDeviceId();
        mqttClient.publish(actuationTopic, str.str());
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

        const bool matched =  actuation.evaluate(telemetry.value, &actuationDecisionCallback);
        if (!matched)
        {
            writeLog("No actuation rule matched for sensor value: " + std::to_string(telemetry.value), LogLevel::Debug);
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

class DeviceVirtualMqttCallback : public MqttCallback
{
public:
    void onMessageReceived(const std::string& topic, const std::string& message) override
    {
        // Topic will be 3 
        // Device configuration
        // Sensing flows
        // Actuation rules
        // We will use Configuration class's loadFromBuffer

        if (topic.find("/device/") != std::string::npos)
        {
            writeLog("Processing device configuration message.", LogLevel::Info);
            if(!devConfig.loadFromBuffer(message))
            {
                writeLog("Failed to load device configuration from message.", LogLevel::Error);
                return;
            }

            // Populate the device with the new configuration
            // Use the Device,ini template to understand the mappings  
            device.loadDeviceDefinition(devConfig);
        }

        if(topic.find("/sensing/") != std::string::npos)
        {
            writeLog("Processing sensing flow message.", LogLevel::Info);
            if(!snsConfig.loadFromBuffer(message))
            {
                writeLog("Failed to load sensing configuration from message.", LogLevel::Error);
                return;
            }
            device.loadSensingFlowDefinition(snsConfig);
        }

        if(topic.find("/actuation/") != std::string::npos)
        {
            writeLog("Processing actuation rule message.", LogLevel::Info);
            if(!actConfig.loadFromBuffer(message))
            {
                writeLog("Failed to load actuation configuration from message.", LogLevel::Error);
                return;
            }
            device.loadActuationRuleDefinition(actConfig);
        }
        
        if(topic.find("/start/") != std::string::npos)
        {
            writeLog("Starting simulation of sensing flow", LogLevel::Info);
            device.startSensingSimulation(&telemetryCallback);
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

    // 1 We will process the application settings 
    // Parse the configuration values for MQTT broker address, port, client ID, username, and password
    brokerAddress = appConfig.getValue("MQTT", "BrokerAddress", "127.0.0.1");
    port = std::stoi(appConfig.getValue("MQTT", "Port", "1883"));
    clientId = appConfig.getValue("MQTT", "ClientId", "DeviceVirtual");
    username = appConfig.getValue("MQTT", "Username", "");
    password = appConfig.getValue("MQTT", "Password", "");
    topic = appConfig.getValue("MQTT", "topic", "devicevirtual");

    // Intialize the sensing mqtt client
    mqttClient.Initialize(brokerAddress, port, clientId, username, password, &callback);
    mqttClient.connect(1, 3600);

    telemetryCallback.setClient(&mqttClient);
    telemetryCallback.setPublishTopic(topic);
    device.startSensingSimulation(&telemetryCallback);

    // Subscribe only to the telemetry topic. Actuation result updates use a dedicated topic so we do not re-enter the sensing callback loop.
    mqttClient.subscribe(topic+"/device/#");
    mqttClient.subscribe(topic+"/sensing/#");
    mqttClient.subscribe(topic+"/actuation/#");
    mqttClient.subscribe(topic+"/start/#");

    while(running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
 
    mqttClient.disconnect();
    writeLog("MQTT clients disconnected.", LogLevel::Info);

    return 0;
}
