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
    void setGeneratorModelType(GeneratorModelType generatorModelType);
    GeneratorModelType getGeneratorModelType();
    bool loadSensorDefinition(const std::filesystem::path& flowFile);
    bool startSimulation(SensorCallback* callback);
private:
    unsigned long getRandomValueInRange(unsigned long min, unsigned long max);

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
    GeneratorModelType _generatorModelType;
};


#endif