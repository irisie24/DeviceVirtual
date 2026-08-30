/*
* The entry point of the HTTP REST API server application.
* All code follows Pascal case and Allman Style.
*/

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sstream>
#include <list>
#include "Mqtt.hpp"
#include "Application.hpp"
#include "SignalHandler.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"
#include "Actuation.hpp"

std::list<std::filesystem::path> discoverRuleFiles(const std::string& configDirectory);
bool tryExtractTelemetryValue(const std::string& message, long& sensorValue);

class ActuationExecutionCallback : public ActuationCallback
{
public:
    void onActionTriggered(const ActuationDecision& decision) override
    {
        std::string actionType = "unknown";
        if (decision.actionType == ActionGpio)
        {
            actionType = "gpio";
        }
        else if (decision.actionType == ActionPwm)
        {
            actionType = "pwm";
        }

        std::cout << "Action triggered - RuleId: " << decision.ruleId
                  << ", ActionType: " << actionType
                  << ", IO: " << decision.actionIo
                  << ", Value: " << decision.actionValue
                  << std::endl;
    }

    void onRuleEvaluationComplete(unsigned long sensorValue) override
    {
        std::cout << "Rule evaluation complete for value: " << sensorValue << std::endl;
    }
};

class TestMqttCallback : public MqttCallback
{
public:
    TestMqttCallback()
        : _actuators(nullptr)
        , _executionCallback(nullptr)
    {
    }

    void setActuators(std::list<Actuation>* actuators)
    {
        _actuators = actuators;
    }

    void setExecutionCallback(ActuationExecutionCallback* callback)
    {
        _executionCallback = callback;
    }

    void onMessageReceived(const std::string& topic, const std::string& message) override
    {
        std::cout << "Received message on topic '" << topic << "': " << message << std::endl;

        if (_actuators == nullptr || _executionCallback == nullptr)
        {
            return;
        }

        long parsedValue = 0;
        if (!tryExtractTelemetryValue(message, parsedValue))
        {
            std::cout << "Skipping message. Telemetry value was not found." << std::endl;
            return;
        }

        unsigned long sensorValue = 0;
        if (parsedValue > 0)
        {
            sensorValue = static_cast<unsigned long>(parsedValue);
        }

        for (std::list<Actuation>::iterator it = _actuators->begin(); it != _actuators->end(); ++it)
        {
            it->evaluate(sensorValue, _executionCallback);
        }
    }

    void onConnectionLost() override
    {
        std::cout << "MQTT connection lost." << std::endl;
    }

    void onConnected() override
    {
        std::cout << "MQTT connected." << std::endl;
    }

private:
    std::list<Actuation>* _actuators;
    ActuationExecutionCallback* _executionCallback;
};

class VirtualActuatorSignalHandler : public SignalCallback
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

Configuration appConfig;
MqttClient client;
SignalHandler signalHandler;

TestMqttCallback callback;
VirtualActuatorSignalHandler sgcallback;
ActuationExecutionCallback actuationExecutionCallback;

int main(int argc, char* argv[])
{
    signalHandler.registerCallbackClient(&sgcallback);

    appConfig.setFileName("VirtualSensors.ini");
    if (!appConfig.loadConfiguration())
    {
        writeLog("Failed to load configuration.", LogLevel::Error);
        return -1;
    }

    std::string configDirectory = appConfig.getConfigDirectory();
    std::list<std::filesystem::path> ruleFiles = discoverRuleFiles(configDirectory);

    std::list<Actuation> actuators;
    for (std::list<std::filesystem::path>::const_iterator it = ruleFiles.begin(); it != ruleFiles.end(); ++it)
    {
        Actuation actuation;
        if (actuation.loadRuleDefinition(*it))
        {
            actuators.push_back(actuation);
            writeLog("Loaded rule definition: " + it->filename().string(), LogLevel::Info);
        }
        else
        {
            writeLog("Failed to load rule definition: " + it->string(), LogLevel::Error);
        }
    }

    if (actuators.empty())
    {
        writeLog("No rule definitions were loaded.", LogLevel::Error);
        return -1;
    }

    std::string brokerAddress = appConfig.getValue("MQTT", "BrokerAddress", "broker.emqx.io");
    int port = std::stoi(appConfig.getValue("MQTT", "Port", "1883"));
    std::string clientId = appConfig.getValue("MQTT", "ClientId", "DeviceVirtualActuatorsClient");
    std::string username = appConfig.getValue("MQTT", "Username", "");
    std::string password = appConfig.getValue("MQTT", "Password", "");
    std::string mqttSubscribeTopic = appConfig.getValue("MQTT", "subscribetopic", "devicevirtual/telemetry");

    if (brokerAddress.empty())
    {
        brokerAddress = "broker.emqx.io";
    }
    if (clientId.empty())
    {
        clientId = "DeviceVirtualActuatorsClient";
    }
    if (mqttSubscribeTopic.empty())
    {
        mqttSubscribeTopic = "devicevirtual/telemetry";
    }

    callback.setActuators(&actuators);
    callback.setExecutionCallback(&actuationExecutionCallback);

    MqttClient client(brokerAddress, port, clientId, username, password, &callback);

    std::cout << "Connecting to EMQX broker..." << std::endl;
    client.connect(1, 10);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!mqttSubscribeTopic.empty())
    {
        client.subscribe(mqttSubscribeTopic);
    }

    std::cout << "Waiting for MQTT activity..." << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    client.disconnect();
    std::cout << "MQTT test finished." << std::endl;
    return 0;
}

std::list<std::filesystem::path> discoverRuleFiles(const std::string& configDirectory)
{
    std::list<std::filesystem::path> ruleFiles;

    if (configDirectory.empty())
    {
        return ruleFiles;
    }

    std::filesystem::path directoryPath(configDirectory);
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
    {
        return ruleFiles;
    }

    for (std::filesystem::directory_iterator it(directoryPath); it != std::filesystem::directory_iterator(); ++it)
    {
        if (!it->is_regular_file())
        {
            continue;
        }

        if (it->path().extension() == ".rule")
        {
            ruleFiles.push_back(it->path());
        }
    }

    ruleFiles.sort();
    return ruleFiles;
}

bool tryExtractTelemetryValue(const std::string& message, long& sensorValue)
{
    sensorValue = 0;

    // Expected telemetry payload format: timestamp=...,value=...,scale=...,unit=...
    size_t valuePos = message.find("value=");
    if (valuePos == std::string::npos)
    {
        return false;
    }

    valuePos += 6;
    size_t valueEnd = message.find(',', valuePos);
    std::string valueText;
    if (valueEnd == std::string::npos)
    {
        valueText = message.substr(valuePos);
    }
    else
    {
        valueText = message.substr(valuePos, valueEnd - valuePos);
    }

    std::stringstream stream(valueText);
    stream >> sensorValue;
    return !stream.fail();
}