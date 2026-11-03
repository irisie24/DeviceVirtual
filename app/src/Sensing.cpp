#include "Sensing.hpp"
#include "Configuration.hpp"
#include "Application.hpp"
#include <vector>
#include <sstream>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <filesystem>

std::string epochToDateTime(const unsigned long epoch);
unsigned long parseUnsignedLong(const std::string& value);
long parseLong(const std::string& value);
SimulationMode parseSimulationMode(const std::string& value);
GeneratorModelType parseGeneratorModelType(const std::string& value);
FlowType parseFlowType(const std::string& value);
std::string flowTypeToString(FlowType flowType);
unsigned long parseUnixEpoch(const std::string& value);

/*
 * Notes on simulation logic
 * Normal and Baseline flows generate data with the max and min range
 * A baseline flow must be present and be at the start of the simulation, if later flows have anamoly types.
 * All anamoly flows (Spikes, Dead, Stuck, Offset, Noise, Drift) must have a baseline flow before them in the simulation.
 * Normal flows are simple value between min and max range, with no anamolies.
 * A simulation run can have just the normal flows. This is permitted and will generate telemetry data with no anamolies.
 * A simulation run can have just the baseline flow. This is permitted and will generate telemetry data with no anamolies.
 * An anamoly flow can only be 2nd in sequence with the baseline flow at the start.
 */

Sensing::Sensing()
{
    _deviceId = "UnknownDevice";
    _numFlows = 0;
    _repeatFlowSequence = false;
    _simulationMode = SimulationModeDataset;
    _generationModelType = ModelUnknown;
    _sensingRuntimeConfiguration.repeatFlowSequence = false;
    _sensingRuntimeConfiguration.flowSequenceEnd = 0;
    _sensingRuntimeConfiguration.simulationMode = SimulationModeDataset;
    _sensingRuntimeConfiguration.generatorModelType = ModelUnknown;
    _sensingRuntimeConfiguration.sensorOperatingMin = 0;
    _sensingRuntimeConfiguration.sensorOperatingMax = 0;
    _sensingRuntimeConfiguration.telemetryScale = 0;
    _sensingRuntimeConfiguration.telemetryUnitOfMeasurement = "";
}

Sensing::~Sensing()
{
}

void Sensing::addFlow(Flow flow)
{
    _flows.push_back(flow);
}

std::list<Flow> Sensing::getFlows()
{
    return _flows;
}

void Sensing::setFlowSetName(const std::string& flowSetName)
{
	std::string datapath = resolveDataPath();
    std::filesystem::create_directories(datapath);
    _flowSetName = datapath + "/" +flowSetName + ".csv"; // Append .csv to the flow set name
}

void Sensing::setDeviceId(const std::string& deviceId)
{
    _deviceId = deviceId;
}

void Sensing::setNumFlows(unsigned long numFlows)
{
    _numFlows = numFlows;
}

unsigned long Sensing::getNumFlows() const
{
    return _numFlows;
}

void Sensing::setRepeatFlowSequence(bool repeatFlowSequence)
{
    _repeatFlowSequence = repeatFlowSequence;
    _sensingRuntimeConfiguration.repeatFlowSequence = _repeatFlowSequence;
}

bool Sensing::getRepeatFlowSequence() const
{
    return _repeatFlowSequence;
}

void Sensing::setFlowSequenceEnd(unsigned long flowSequenceEnd)
{
    _sensingRuntimeConfiguration.flowSequenceEnd = flowSequenceEnd;
}

unsigned long Sensing::getFlowSequenceEnd() const
{
    return _sensingRuntimeConfiguration.flowSequenceEnd;
}

void Sensing::setSimulationMode(SimulationMode simulationMode)
{
    _simulationMode = simulationMode;
    _sensingRuntimeConfiguration.simulationMode = _simulationMode;
}

SimulationMode Sensing::getSimulationMode() const
{
    return _simulationMode;
}

void Sensing::setGenerationModelType(GeneratorModelType generationModelType)
{
    _generationModelType = generationModelType;
    _sensingRuntimeConfiguration.generatorModelType = _generationModelType;
}

GeneratorModelType Sensing::getGenerationModelType() const
{
    return _generationModelType;
}

void Sensing::setRuntimeConfiguration(const SensingRuntimeConfiguration& runtimeConfiguration)
{
    _sensingRuntimeConfiguration = runtimeConfiguration;
    _repeatFlowSequence = _sensingRuntimeConfiguration.repeatFlowSequence;
    _simulationMode = _sensingRuntimeConfiguration.simulationMode;
    _generationModelType = _sensingRuntimeConfiguration.generatorModelType;
}

const SensingRuntimeConfiguration& Sensing::getRuntimeConfiguration() const
{
    return _sensingRuntimeConfiguration;
}

void Sensing::normalizeFlowRanges(const SensingRuntimeConfiguration& runtimeConfiguration)
{
    if (runtimeConfiguration.sensorOperatingMin == 0 && runtimeConfiguration.sensorOperatingMax == 0)
    {
        return;
    }

    const long lowerBound = std::min(runtimeConfiguration.sensorOperatingMin, runtimeConfiguration.sensorOperatingMax);
    const long upperBound = std::max(runtimeConfiguration.sensorOperatingMin, runtimeConfiguration.sensorOperatingMax);

    for (Flow& flow : _flows)
    {
        long normalizedMin = static_cast<long>(flow.minimum);
        long normalizedMax = static_cast<long>(flow.maximum);

        if (normalizedMin < lowerBound)
        {
            normalizedMin = lowerBound;
        }
        if (normalizedMax > upperBound)
        {
            normalizedMax = upperBound;
        }
        if (normalizedMin > normalizedMax)
        {
            normalizedMin = lowerBound;
            normalizedMax = upperBound;
        }

        flow.minimum = normalizedMin;
        flow.maximum = normalizedMax;
    }
}

bool Sensing::loadSensorDefinition(Configuration& flowConfiguration)
{
    _flows.clear();

    if (flowConfiguration.isSection("General"))
    {
        setNumFlows(parseUnsignedLong(flowConfiguration.getValue("General", "num_flows", "0")));
        setRepeatFlowSequence(flowConfiguration.getValue("General", "repeat_flow_sequence", "0") == "1");
        setFlowSequenceEnd(parseUnixEpoch(flowConfiguration.getValue("General", "flow_sequence_end", "1970/01/01 00:00:00")));
        setSimulationMode(parseSimulationMode(flowConfiguration.getValue("General", "simulation_mode", "dataset")));
        setGenerationModelType(parseGeneratorModelType(flowConfiguration.getValue("General", "generation_model", "deterministic_pattern")));
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
        flow.minimum = parseLong(flowConfiguration.getValue(section, "min", "0"));
        flow.maximum = parseLong(flowConfiguration.getValue(section, "max", "100"));
        flow.duration = parseUnsignedLong(flowConfiguration.getValue(section, "intervalsecs", "300"));
        flow.unixepochstart = parseUnixEpoch(flowConfiguration.getValue(section, "start", "1970/01/01 00:00:00"));
        flow.unixepochend = parseUnixEpoch(flowConfiguration.getValue(section, "end", "1970/01/01 00:00:00"));

        addFlow(flow);
    }

    return !_flows.empty();
}

bool Sensing::removeFlow(Flow flow)
{
    for (std::list<Flow>::iterator it = _flows.begin(); it != _flows.end(); ++it)
    {
        if (it->type == flow.type &&
            it->duration == flow.duration &&
            it->unixepochstart == flow.unixepochstart &&
            it->minimum == flow.minimum &&
            it->maximum == flow.maximum )
        {
            _flows.erase(it);
            return true;
        }
    }
    return false;
}

bool Sensing::startSimulation(const SensingRuntimeConfiguration& runtimeConfiguration, SensorCallback* callback)
{
    _baselineTelemetry.clear();
    normalizeFlowRanges(runtimeConfiguration);

    if (runtimeConfiguration.simulationMode == SimulationModeRealtime)
    {
        std::thread simulationThread(Sensing::SimulationThreadProc, this, callback, runtimeConfiguration);
        simulationThread.detach();
    }
    else
    {
        SimulationThreadProc(this, callback, runtimeConfiguration);
    }

    return true;
}

void Sensing::SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback, SensingRuntimeConfiguration runtimeConfiguration)
{
    if (callback == nullptr)
    {
        return;
    }

    size_t flowPos = 0;

    for (Flow flow : sensingInstance->getFlows())
    {
        // If we are at flow position 1 and flow position 0 was BaseLine type, then we need to generate the statistics
        if(flowPos == 1 && sensingInstance->_flows.front().type == BaseLine)
        {
            sensingInstance->computeMaxOfBaseline();
            sensingInstance->computeMinOfBaseline();
            sensingInstance->computeMedianOfBaseline();
            sensingInstance->computeAverageOfBaseline();
        }

        // Based on the flow's start and end times we set up a for loop to simulate the telemetry data
        if(flow.unixepochstart >= flow.unixepochend)
        {
            // Invalid flow, skip to the next one
            continue;
        }

        if (flow.duration == 0)
        {
            continue;
        }

        // Setup some handy variables for later use in the loop

        // For Stuck flow type, we need a value to stick to
        long stuckValue = sensingInstance->getRandomValueInRange(sensingInstance->_minBaseline, sensingInstance->_maxBaseline);

        // Calculate the total sample count based on time range and duration.
        unsigned long sampleCount = ((flow.unixepochend - flow.unixepochstart) / flow.duration) + 1;
        if (sampleCount == 0)
        {
            sampleCount = 1;
        }

        long offsetValue = 0;
        // Offset will be = median + plus or minus decided by a random bool + random percent of the median value
        const long medianOffsetRange = std::max<long>(0, sensingInstance->_medianBaseline);
        offsetValue = (sensingInstance->_medianBaseline + sensingInstance->getRandomValueInRange(0, medianOffsetRange) * (sensingInstance->getRandomBoolean() ? 1 : -1));

        // Distribution vector to set the elements which need to set with fault values
        // We will set the vector with true using generation model algorithms

        // A binary array of boolean that are set to true act as a distribution set in following modes.
        // The range is iterated from start to end as the value array and the dustribution array are matched index to index.
        // 1. Normal flows - No effect
        // 2. Baseline flows - No effect
        // 3. Spike - If the binary index indicates true, then spike to be generated, or else generate a normal value
        // 4. Offset - No effect. Offset is calculate at the outset before starting the value loop
        // 5. Stepped Drift - The boolean true position serve as checkpoints. At checkpoints values tend to move away from the previous median
        // 6. Linear Drift - The boolean true position serve as checkpoints. At checkpoints values may change the angle of slope as they deviate from baseline median
        // 7. Stuck - No currently used, but the boolean true at the median of the random distribution set will be treated as the checkpoint where values get stuck.
        // 8. Zero - No currently used, but the boolean true at the median of the random distribution set will be treated as the checkpoint where values become 0.
        // 9. Noise or Jitter - If the binary index indicates true, then noise of jitter to be generated, or else generate a normal value

        std::vector<bool> distibution(static_cast<size_t>(sampleCount), false);
        std::fill(distibution.begin(), distibution.end(), false);

        switch(runtimeConfiguration.generatorModelType)
        {
            case Bernoulli:
            {
                // Implement Bernoulli generation model logic here
                sensingInstance->SetWithBernoulliDistribution(distibution);
                break;
            }
             case SemiMarkov:
            {
                sensingInstance->SetWithSemiMarkovDistribution(distibution);
                break;
            }
            case RenewalProcess:
            {
                sensingInstance->SetWithRenewalProcessDistribution(distibution);
                break;
            }
            case PoissonOnset:
            {
                sensingInstance->SetWithPoissonOnsetDistribution(distibution);
                break;
            }
            case NegativeBinomialRuns:
            {
                sensingInstance->SetWithNegativeBinomialRunsDistribution(distibution);
                break;
            }
            case HiddenMarkov:
            {
                sensingInstance->SetWithHiddenMarkovDistribution(distibution);
                break;
            }
            case BetaBernoulliMixture:
            {
                sensingInstance->SetWithBetaBernoulliMixtureDistribution(distibution);
                break;
            }
            case BlockSegment:
            {
                sensingInstance->SetWithBlockSegmentDistribution(distibution);
                break;
            }
            case DeterministicPattern:
            {
                sensingInstance->SetWithDeterministicPatternDistribution(distibution);
                break;
            }
            case MarkovTwoState:
            {
                sensingInstance->SetWithMarkovTwoStateDistribution(distibution);
                break;
            }
            case ModelUnknown:
            default:
            {
                // Unknown model fallback: use Bernoulli for compatibility.
                sensingInstance->SetWithBernoulliDistribution(distibution);
                break;
            }
        }

        unsigned long startTime = flow.unixepochstart;

        // Per-flow dataset output: generate a numbered CSV file for this flow when in dataset mode.
        std::ofstream perFlowFile;
        std::string perFlowFilename;
        if (runtimeConfiguration.simulationMode == SimulationModeDataset)
        {
            // Prefer the datasetfilename from the Flow if present; otherwise derive from the overall flow set name.
            std::filesystem::path basePath;
            if (!flow.datasetfilename.empty())
            {
                basePath = std::filesystem::path(flow.datasetfilename);
            }
            else
            {
                basePath = std::filesystem::path(sensingInstance->_flowSetName);
            }

            std::filesystem::path parent = basePath.parent_path();
            std::string filename = basePath.filename().string(); // e.g., xyz.flow or xyz.csv

            // If basePath was the flow set name (which likely ends with .csv), remove that extension to avoid double .csv
            if (basePath == std::filesystem::path(sensingInstance->_flowSetName))
            {
                filename = basePath.stem().string(); // use stem, e.g., xyz from xyz.csv
            }

            std::filesystem::path candidate;
            //int idx = 1;
            //do
            //{
                // Build name like: xyz.flow1.csv (if filename is "xyz.flow") or xyz1.csv (if filename is "xyz")
            //    candidate = parent / (filename + "_flow_" + std::to_string(idx) + std::string(".csv"));
            //    idx++;
            //} while (std::filesystem::exists(candidate));

            candidate = parent / (filename + "_flow_" + flowTypeToString(flow.type) + std::string(".csv"));


            perFlowFilename = candidate.generic_string();
            perFlowFile.open(perFlowFilename);
            if (!perFlowFile.is_open())
            {
                callback->onMessage(sensingInstance->_deviceId, "Failed to open telemetry file for writing: " + perFlowFilename);
                // continue without writing this flow's data
            }
            else
            {
                callback->onMessage(sensingInstance->_deviceId, "Opened telemetry file for writing: " + perFlowFilename);
                //perFlowFile << "Timestamp,Value,Scale,UnitOfMeasurement" << std::endl;
                perFlowFile << "Timestamp,Value" << std::endl;
            }
        }

        for (unsigned long t = flow.unixepochstart, ct = 0; t <= flow.unixepochend; t += flow.duration, ct++)
        {
            Telemetry telemetry;
            telemetry.timestamp = t; // Use the flow's timestamp in dataset mode
            telemetry.value = 0;
            telemetry.scale = runtimeConfiguration.telemetryScale;
            telemetry.unitofmeasurement = runtimeConfiguration.telemetryUnitOfMeasurement;
            bool isAnomalySample = false;
            if (ct < static_cast<unsigned long>(distibution.size()))
            {
                isAnomalySample = distibution[static_cast<size_t>(ct)];
            }
            
            // Value generation based on flow type
            switch (flow.type)
            {
                case BaseLine:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum); // Generate a random value within the min and max range for Baseline
                    sensingInstance->_baselineTelemetry.push_back(telemetry);
                    break;
                }
                case Normal:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum); // Use the flow's value for Normal
                    break;
                }
                case Spikes:
                {
                    if (isAnomalySample)
                    {
                        telemetry.value = sensingInstance->_maxBaseline + sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum); // Generate a spike value
                    }
                    else
                    {
                        telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum); // Generate a normal value
                    }
                    break;
                }
                case Dead:
                {
                    if (ct > sampleCount/2)
                    {
                        telemetry.value = 0; // Simulate a dead sample with value 0
                    }
                    else
                    {
                        telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    }
                    break;
                }
                case Stuck:
                {
                    if (ct > sampleCount/2)
                    {
                        telemetry.value = stuckValue; // Simulate a stuck sample with the same value
                    }
                    else
                    {
                        telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    }
                    break;
                }
                case Offset:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum + offsetValue, flow.maximum + offsetValue);

                    break;
                }
                case Noise:
                {
                    long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        const long jitter = ((flow.maximum - flow.minimum) / 10) + 1;
                        const unsigned long direction = static_cast<unsigned long>(rand() % 2);
                        if (direction == 0)
                        {
                            telemetry.value = baseValue + jitter;
                        }
                        else
                        {
                            if (baseValue > jitter)
                            {
                                telemetry.value = baseValue - jitter;
                            }
                            else
                            {
                                telemetry.value = 0;
                            }
                        }
                    }
                    else
                    {
                        telemetry.value = baseValue;
                    }
                    break;
                }
                case DriftLinear:
                {
                    long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        const long maxDrift = ((flow.maximum - flow.minimum) / 3) + 1;
                        const long drift = (static_cast<long>(ct) * maxDrift) / static_cast<long>(sampleCount);
                        telemetry.value = baseValue + drift;
                    }
                    else
                    {
                        telemetry.value = baseValue;
                    }
                    break;
                }
                case DriftStepped:
                {
                    long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        const unsigned long stepWindow = (sampleCount / 5) + 1;
                        const unsigned long stepIndex = ct / stepWindow;
                        const long stepSize = ((flow.maximum - flow.minimum) / 8) + 1;
                        telemetry.value = baseValue + (static_cast<long>(stepIndex) * stepSize);
                    }
                    else
                    {
                        telemetry.value = baseValue;
                    }
                    break;
                }
                case FlowUnknown:
                default:
                {
                    // Default case, should not happen, but just in case
                    telemetry.value = 0; 
                    break;
                }
            }

            callback->onTelemetryReceived(sensingInstance->_deviceId, telemetry, runtimeConfiguration.simulationMode);

            if(runtimeConfiguration.simulationMode == SimulationModeRealtime)
            {
                std::this_thread::sleep_for(std::chrono::seconds(flow.duration)); // Simulate a delay based on the flow's duration
            }
            else
            {
                if(runtimeConfiguration.simulationMode == SimulationModeDataset)
                {
                    // Write to per-flow file if available (new behavior)
                    // Old behavior wrote to a single telemetryFile for the whole run.
                    // telemetryFile << telemetry.timestamp << "," << telemetry.value << "," << telemetry.scale << "," << telemetry.unitofmeasurement << std::endl;
                    if (perFlowFile.is_open())
                    {
                        //perFlowFile << telemetry.timestamp << "," << telemetry.value << "," << telemetry.scale << "," << telemetry.unitofmeasurement << std::endl;
                        perFlowFile << epochToDateTime(telemetry.timestamp) << "," << telemetry.value << std::endl;
                    }
                }
            }
        }

        // Close and notify per-flow file when in dataset mode
        if (runtimeConfiguration.simulationMode == SimulationModeDataset)
        {
            if (perFlowFile.is_open())
            {
                perFlowFile.close();
                callback->onTelemetryGenerationComplete(sensingInstance->_deviceId, perFlowFilename);
            }
        }

        flowPos++;
    }
}

void Sensing::computeMaxOfBaseline()
{
    if (_baselineTelemetry.empty())
    {
        _maxBaseline = 0;
        return;
    }

    long maxValue = _baselineTelemetry.front().value;
    for (std::list<Telemetry>::const_iterator it = _baselineTelemetry.begin(); it != _baselineTelemetry.end(); ++it)
    {
        if (it->value > maxValue)
        {
            maxValue = it->value;
        }
    }

    _maxBaseline = maxValue;
    return;
}

void Sensing::computeMinOfBaseline()
{
    if (_baselineTelemetry.empty())
    {
        _minBaseline = 0;
        return;
    }

    long minValue = _baselineTelemetry.front().value;
    for (std::list<Telemetry>::const_iterator it = _baselineTelemetry.begin(); it != _baselineTelemetry.end(); ++it)
    {
        if (it->value < minValue)
        {
            minValue = it->value;
        }
    }

    _minBaseline = minValue;
    return;
}

void Sensing::computeMedianOfBaseline()
{
    if (_baselineTelemetry.empty())
    {
        _medianBaseline = 0;
        return;
    }

    std::list<long> values;
    for (std::list<Telemetry>::const_iterator it = _baselineTelemetry.begin(); it != _baselineTelemetry.end(); ++it)
    {
        values.push_back(it->value);
    }

    values.sort();

    const size_t count = values.size();
    const size_t middle = count / 2;

    if (count % 2 == 0)
    {
        std::list<long>::const_iterator it = values.begin();
        std::advance(it, static_cast<long>(middle - 1));
        long first = *it;

        ++it;
        long second = *it;

        _medianBaseline = (first + second) / 2;
        return;
    }

    std::list<long>::const_iterator it = values.begin();
    std::advance(it, static_cast<long>(middle));
    _medianBaseline = *it;
    return;
}

void Sensing::computeAverageOfBaseline()
{
    if (_baselineTelemetry.empty())
    {
        _averageBaseline = 0;
        return;
    }

    long total = 0;
    size_t count = 0;
    for (std::list<Telemetry>::const_iterator it = _baselineTelemetry.begin(); it != _baselineTelemetry.end(); ++it)
    {
        total += it->value;
        ++count;
    }

    if (count == 0)
    {
        _averageBaseline = 0;
        return;
    }

    _averageBaseline = total / static_cast<long>(count);
    return;
}

long Sensing::getRandomValueInRange(long min, long max)
{
    if (min >= max)
    {
        return min;
    }

    const long range = max - min + 1;
    return min + static_cast<long>(rand() % static_cast<unsigned long>(range));
}

bool Sensing::getRandomBoolean()
{
    return (rand() % 2) == 1;
}

// Internal helper functions for Sensing class

void Sensing::SetWithBernoulliDistribution(std::vector<bool> &set)
{
    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        *it = (rand() % 2) == 1;
    }
}

void Sensing::SetWithMarkovTwoStateDistribution(std::vector<bool> &set)
{
    bool state = false;
    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        unsigned long roll = static_cast<unsigned long>(rand() % 100);
        if (state == false)
        {
            if (roll < 10)
            {
                state = true;
            }
        }
        else
        {
            if (roll < 30)
            {
                state = false;
            }
        }
        *it = state;
    }
}

void Sensing::SetWithSemiMarkovDistribution(std::vector<bool> &set)
{
    bool state = false;
    unsigned long remaining = 0;

    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        if (remaining == 0)
        {
            state = !state;
            if (state)
            {
                remaining = static_cast<unsigned long>((rand() % 6) + 2);
            }
            else
            {
                remaining = static_cast<unsigned long>((rand() % 12) + 3);
            }
        }

        *it = state;
        remaining--;
    }
}

void Sensing::SetWithRenewalProcessDistribution(std::vector<bool> &set)
{
    unsigned long nextRenewal = static_cast<unsigned long>((rand() % 8) + 2);
    unsigned long index = 0;
    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        *it = false;
        if (index == nextRenewal)
        {
            *it = true;
            nextRenewal = index + static_cast<unsigned long>((rand() % 8) + 2);
        }
        index++;
    }
}

void Sensing::SetWithPoissonOnsetDistribution(std::vector<bool> &set)
{
    unsigned long burstRemaining = 0;
    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        if (burstRemaining == 0)
        {
            unsigned long onsetRoll = static_cast<unsigned long>(rand() % 100);
            if (onsetRoll < 8)
            {
                burstRemaining = static_cast<unsigned long>((rand() % 4) + 1);
            }
        }

        if (burstRemaining > 0)
        {
            *it = true;
            burstRemaining--;
        }
        else
        {
            *it = false;
        }
    }
}

void Sensing::SetWithNegativeBinomialRunsDistribution(std::vector<bool> &set)
{
    unsigned long index = 0;
    while (index < static_cast<unsigned long>(set.size()))
    {
        unsigned long gap = static_cast<unsigned long>((rand() % 10) + 1);
        index += gap;
        if (index >= static_cast<unsigned long>(set.size()))
        {
            break;
        }

        unsigned long runLength = static_cast<unsigned long>((rand() % 6) + 2);
        unsigned long runEnd = index + runLength;
        while (index < runEnd && index < static_cast<unsigned long>(set.size()))
        {
            set[static_cast<size_t>(index)] = true;
            index++;
        }
    }
}

void Sensing::SetWithHiddenMarkovDistribution(std::vector<bool> &set)
{
    unsigned long hiddenState = 0;
    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        unsigned long transitionRoll = static_cast<unsigned long>(rand() % 100);
        if (hiddenState == 0)
        {
            if (transitionRoll < 12)
            {
                hiddenState = 1;
            }
        }
        else
        {
            if (transitionRoll < 25)
            {
                hiddenState = 0;
            }
        }

        if (hiddenState == 1)
        {
            *it = (rand() % 100) < 75;
        }
        else
        {
            *it = (rand() % 100) < 10;
        }
    }
}

void Sensing::SetWithBetaBernoulliMixtureDistribution(std::vector<bool> &set)
{
    unsigned long mixtureRoll = static_cast<unsigned long>(rand() % 100);
    unsigned long pTrue = 15;
    if (mixtureRoll < 30)
    {
        pTrue = 70;
    }

    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        unsigned long roll = static_cast<unsigned long>(rand() % 100);
        *it = (roll < pTrue);
    }
}

void Sensing::SetWithBlockSegmentDistribution(std::vector<bool> &set)
{
    if (set.empty())
    {
        return;
    }

    unsigned long size = static_cast<unsigned long>(set.size());
    unsigned long minBlock = (size / 10) + 1;
    unsigned long maxBlock = (size / 3) + 1;
    unsigned long blockLength = minBlock;
    if (maxBlock > minBlock)
    {
        blockLength = minBlock + static_cast<unsigned long>(rand() % (maxBlock - minBlock + 1));
    }

    if (blockLength > size)
    {
        blockLength = size;
    }

    unsigned long start = 0;
    if (size > blockLength)
    {
        start = static_cast<unsigned long>(rand() % (size - blockLength + 1));
    }

    for (unsigned long i = start; i < (start + blockLength); ++i)
    {
        set[static_cast<size_t>(i)] = true;
    }
}

void Sensing::SetWithDeterministicPatternDistribution(std::vector<bool> &set)
{
    unsigned long period = 6;
    unsigned long width = 2;
    unsigned long index = 0;

    for (std::vector<bool>::iterator it = set.begin(); it != set.end(); ++it)
    {
        unsigned long inPeriod = index % period;
        *it = (inPeriod < width);
        index++;
    }
}

std::string epochToDateTime(const unsigned long epoch)
{
    long long epochSeconds = static_cast<long long>(epoch);
    std::chrono::sys_seconds tp{ std::chrono::seconds(epochSeconds) };
    return std::format("{:%Y/%m/%d %H:%M:%S}", tp);
}

unsigned long parseUnsignedLong(const std::string& value)
{
    std::stringstream stream(value);
    unsigned long result = 0;
    stream >> result;
    return result;
}

long parseLong(const std::string& value)
{
    std::stringstream stream(value);
    long result = 0;
    stream >> result;
    return result;
}

SimulationMode parseSimulationMode(const std::string& value)
{
    if (value == "realtime")
    {
        return SimulationModeRealtime;
    }

    return SimulationModeDataset;
}

GeneratorModelType parseGeneratorModelType(const std::string& value)
{
    if (value == "bernoulli")
    {
        return Bernoulli;
    }
    if (value == "markov_two_state" || value == "markov")
    {
        return MarkovTwoState;
    }
    if (value == "semi_markov")
    {
        return SemiMarkov;
    }
    if (value == "renewal_process")
    {
        return RenewalProcess;
    }
    if (value == "poisson_onset")
    {
        return PoissonOnset;
    }
    if (value == "negative_binomial_runs")
    {
        return NegativeBinomialRuns;
    }
    if (value == "hidden_markov")
    {
        return HiddenMarkov;
    }
    if (value == "beta_bernoulli_mixture")
    {
        return BetaBernoulliMixture;
    }
    if (value == "block_segment")
    {
        return BlockSegment;
    }
    if (value == "deterministic_pattern" || value == "deterministic")
    {
        return DeterministicPattern;
    }

    return ModelUnknown;
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

    return FlowUnknown;
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

std::string flowTypeToString(FlowType flowType)
{
	switch (flowType)
	{
	case BaseLine:
		return "baseline";
	case Normal:
		return "normal";
	case Spikes:
		return "spikes";
	case Dead:
		return "dead";
	case Stuck:
		return "stuck";
	case Offset:
		return "offset";
	case Noise:
		return "noise";
	case DriftLinear:
		return "driftlinear";
	case DriftStepped:
		return "driftstepped";
	default:
		return "unknown";
	}
}