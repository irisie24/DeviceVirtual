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

typedef enum SimulationMode
{
    SimulationModeDataset,
    SimulationModeRealtime
}SimulationMode;

// Sensor simulation flow type
typedef enum FlowType
{
    BaseLine,
    Normal,
    Spikes,
    Dead,
    Stuck,
    Offset,
    Noise,
    DriftLinear,
    DriftStepped,
    Unknown   
}FlowType;

typedef struct Flow
{
    FlowType type;
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
    virtual void onTelemetryGenerationComplete(const std::string& telemetryfilename) = 0;
};

class Sensing
{
public:
    Sensing();
    ~Sensing();
    void addFlow(Flow flow);
    std::list<Flow> getFlows();
    bool removeFlow(Flow flow);
    void setRepeatable(bool repeatable, unsigned long unixepochend);
    void setSimulationMode(SimulationMode simulationMode);
    void setFlowSetName(const std::string& flowSetName);
    SimulationMode getSimulationMode();
    bool startSimulation(SensorCallback* callback);
private:
    unsigned long getRandomValueInRange(unsigned long min, unsigned long max);

    void computeMaxOfBaseline();
    void computeMinOfBaseline();
    void computeMedianOfBaseline();
    void computeAverageOfBaseline();

    static void SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback);

    std::list<Flow> _flows;
    bool _repeatable;
    unsigned long _unixepochend;
    SimulationMode _simulationMode;
    std::list<Telemetry> _baselineTelemetry;
    unsigned long _maxBaseline;
    unsigned long _minBaseline;
    unsigned long _medianBaseline;
    unsigned long _averageBaseline;
    std::string _flowSetName;
};


#endif