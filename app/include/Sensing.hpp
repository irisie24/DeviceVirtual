#ifndef SENSING_
#define SENSING_

#include <list>
#include <string>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <random>
#include <fstream>
#include <vector>
#include <filesystem>
#include "Configuration.hpp"

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
    FlowUnknown   
}FlowType;

typedef enum GeneratorModelType
{
    Bernoulli,
    MarkovTwoState,
    SemiMarkov,
    RenewalProcess,
    PoissonOnset,
    NegativeBinomialRuns,
    HiddenMarkov,
    BetaBernoulliMixture,
    BlockSegment,
    DeterministicPattern,
    ModelUnknown

}GeneratorModelType;

typedef struct Flow
{
    FlowType type;
    unsigned long duration;
    unsigned long unixepochstart;
    unsigned long unixepochend;
    unsigned long minimum;
    unsigned long maximum;
	std::string datasetfilename;
}Flow;

typedef struct Telemetry
{
    unsigned long timestamp;
    unsigned long value;
    unsigned long scale;
    std::string unitofmeasurement;
}Telemetry;

typedef struct SensingRuntimeConfiguration
{
    bool repeatFlowSequence;
    unsigned long flowSequenceEnd;
    SimulationMode simulationMode;
    GeneratorModelType generatorModelType;
    unsigned long telemetryScale;
    std::string telemetryUnitOfMeasurement;
}SensingRuntimeConfiguration;

class SensorCallback
{
public:
    virtual void onTelemetryReceived(const std::string& deviceid, const Telemetry& telemetry, SimulationMode mode) = 0;
    virtual void onTelemetryGenerationComplete(const std::string& deviceid, const std::string& telemetryfilename) = 0;
    virtual void onMessage(const std::string& deviceid, const std::string& msg) = 0;
};

class Sensing
{
public:
    Sensing();
    ~Sensing();
    void addFlow(Flow flow);
    std::list<Flow> getFlows();
    bool removeFlow(Flow flow);
    void setFlowSetName(const std::string& flowSetName);
    void setDeviceId(const std::string& deviceId);
    void setNumFlows(unsigned long numFlows);
    unsigned long getNumFlows() const;
    void setRepeatFlowSequence(bool repeatFlowSequence);
    bool getRepeatFlowSequence() const;
    void setFlowSequenceEnd(unsigned long flowSequenceEnd);
    unsigned long getFlowSequenceEnd() const;
    void setSimulationMode(SimulationMode simulationMode);
    SimulationMode getSimulationMode() const;
    void setGenerationModelType(GeneratorModelType generationModelType);
    GeneratorModelType getGenerationModelType() const;
    void setRuntimeConfiguration(const SensingRuntimeConfiguration& runtimeConfiguration);
    const SensingRuntimeConfiguration& getRuntimeConfiguration() const;
    bool loadSensorDefinition(Configuration& flowConfiguration);
    bool startSimulation(const SensingRuntimeConfiguration& runtimeConfiguration, SensorCallback* callback);
private:
    unsigned long getRandomValueInRange(unsigned long min, unsigned long max);
    bool getRandomBoolean();

    void SetWithBernoulliDistribution(std::vector<bool> &set);
    void SetWithMarkovTwoStateDistribution(std::vector<bool> &set);
    void SetWithSemiMarkovDistribution(std::vector<bool> &set);
    void SetWithRenewalProcessDistribution(std::vector<bool> &set);
    void SetWithPoissonOnsetDistribution(std::vector<bool> &set);
    void SetWithNegativeBinomialRunsDistribution(std::vector<bool> &set);
    void SetWithHiddenMarkovDistribution(std::vector<bool> &set);
    void SetWithBetaBernoulliMixtureDistribution(std::vector<bool> &set);
    void SetWithBlockSegmentDistribution(std::vector<bool> &set);
    void SetWithDeterministicPatternDistribution(std::vector<bool> &set);

    void computeMaxOfBaseline();
    void computeMinOfBaseline();
    void computeMedianOfBaseline();
    void computeAverageOfBaseline();

    static void SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback, SensingRuntimeConfiguration runtimeConfiguration);

    std::list<Flow> _flows;
    std::list<Telemetry> _baselineTelemetry;
    unsigned long _maxBaseline;
    unsigned long _minBaseline;
    unsigned long _medianBaseline;
    unsigned long _averageBaseline;
    std::string _flowSetName;
    std::string _deviceId;
    unsigned long _numFlows;
    bool _repeatFlowSequence;
    SimulationMode _simulationMode;
    GeneratorModelType _generationModelType;
    SensingRuntimeConfiguration _sensingRuntimeConfiguration;
};


#endif