#ifndef APPLICATION_
#define APPLICATION_

#include "Logger.hpp"
#include "Configuration.hpp"
#include "Mqtt.hpp"
#include "SignalHandler.hpp"
#include "Device.hpp"
#include "Sensing.hpp"
#include "Actuation.hpp"

// This file will contain all application specific includes and definitions. It will be included in the main.cpp file.
extern Logger* loggerRefPtr;
extern Configuration appConfig;
extern Configuration devConfig;
extern Configuration snsConfig;
extern Configuration actConfig;
extern MqttClient sensingClient;
extern MqttClient actuationClient;
extern SignalHandler signalHandler;

extern Device device;
extern Sensing sensing;
extern Actuation actuation;

extern std::string brokerAddress;
extern int port;
extern std::string clientId;
extern std::string username;
extern std::string password;
extern std::string topic;

extern std::string resolveDataPath();
extern std::string getTelemetryFieldValue(const std::string& payload, const std::string& key);
extern bool parseLongValue(const std::string& value, long& result);
extern std::string parseDeviceIdFromTopic(const std::string& mqttTopic);

extern bool running;
    
#endif