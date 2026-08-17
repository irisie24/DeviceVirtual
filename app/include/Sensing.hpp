#ifndef SENSING_
#define SENSING_

#include <list>
#include <string>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>

// Code style fulll Allman style
// Excluded C++ features - exclude auto keyword, exclude lambdas

// Flow type

// Sensor simulation flow type
typedef enum FlowType
{
    BaseLine,
    Spikes,
    DeadF,
    Stuck,
    Offset,
    Noise,
    Drift   
}FlowType;

typedef struct Flow
{
    FlowType type;
    unsigned long value;
    unsigned long scale;
    unsigned long duration;
    unsigned long unixepochstart;
    unsigned long unixepochend;
    unsigned long minimum;
    unsigned long maximum;
    char unitofmeasurement[17];
}Flow;

typedef struct Telemetry
{
    unsigned long timestamp;
    unsigned long value;
    unsigned long scale;
    std::string unitofmeasurement;
}Telemetry;

class SensorCallback
{
public:
    virtual void onTelemetryReceived(const Telemetry& telemetry) = 0;
};

class Sensing
{
public:
    Sensing();
    ~Sensing();
    void addFlow(Flow flow);
    std::list<Flow> getFlows();
    bool removeFlow(Flow flow);
    bool startSimulation(SensorCallback* callback);
private:
    std::list<Flow> _flows;
    SensorCallback* _callbackPtr;
};


#endif