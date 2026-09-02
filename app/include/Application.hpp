#ifndef APPLICATION_
#define APPLICATION_

#include "Logger.hpp"
#include "Configuration.hpp"
#include "Mqtt.hpp"
#include "SignalHandler.hpp"

// This file will contain all application specific includes and definitions. It will be included in the main.cpp file.
extern Logger* loggerRefPtr;
extern Configuration appConfig;
extern MqttClient client;
extern SignalHandler signalHandler;

extern std::string resolveDataPath();

#endif