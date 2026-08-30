#include "Sensing.hpp"
#include "Configuration.hpp"
#include <vector>
#include <sstream>
#include <ctime>
#include <cstring>
#include <iomanip>

namespace
{
    unsigned long parseUnsignedLong(const std::string& value)
    {
        std::stringstream stream(value);
        unsigned long result = 0;
        stream >> result;
        return result;
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
        else if (value == "markov_two_state" || value == "markov")
        {
            return MarkovTwoState;
        }
        else if (value == "semi_markov")
        {
            return SemiMarkov;
        }
        else if (value == "renewal_process")
        {
            return RenewalProcess;
        }
        else if (value == "poisson_onset")
        {
            return PoissonOnset;
        }
        else if (value == "negative_binomial_runs")
        {
            return NegativeBinomialRuns;
        }
        else if (value == "hidden_markov")
        {
            return HiddenMarkov;
        }
        else if (value == "beta_bernoulli_mixture")
        {
            return BetaBernoulliMixture;
        }
        else if (value == "block_segment")
        {
            return BlockSegment;
        }
        else if (value == "deterministic_pattern" || value == "deterministic")
        {
            return DeterministicPattern;
        }

        return ModelUnknown;
    }
}

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
    _repeatable = false;
    _unixepochend = 0;
    _simulationMode = SimulationModeDataset;
    _generatorModelType = ModelUnknown;
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

void Sensing::setRepeatable(bool repeatable, unsigned long unixepochend)
{
    _repeatable = repeatable;
    _unixepochend = unixepochend;
}

void Sensing::setSimulationMode(SimulationMode simulationMode)
{
    _simulationMode = simulationMode;
}

SimulationMode Sensing::getSimulationMode()
{
    return _simulationMode;
}

void Sensing::setFlowSetName(const std::string& flowSetName)
{
    _flowSetName = flowSetName + ".csv"; // Append .csv to the flow set name
}

void Sensing::setGeneratorModelType(GeneratorModelType generatorModelType)
{
    _generatorModelType = generatorModelType;
}

GeneratorModelType Sensing::getGeneratorModelType()
{
    return _generatorModelType;
}

bool Sensing::loadSensorDefinition(const std::filesystem::path& flowFile)
{
    Configuration flowConfiguration;

    if (!flowConfiguration.loadCustomConfiguration(flowFile.string()))
    {
        return false;
    }

    if (flowConfiguration.isSection("General"))
    {
        bool repeatFlowSequence = flowConfiguration.getValue("General", "repeat_flow_sequence", "0") == "1";
        unsigned long flowSequenceEnd = parseUnixEpoch(flowConfiguration.getValue("General", "flow_sequence_end", "1970/01/01 00:00:00"));
        SimulationMode simulationMode = parseSimulationMode(flowConfiguration.getValue("General", "simulation_mode", "dataset"));

        std::string generatorModelValue = flowConfiguration.getValue("General", "generator_model_type", "");
        if (generatorModelValue.empty())
        {
            // Backward compatibility: older flow files use generation_model.
            generatorModelValue = flowConfiguration.getValue("General", "generation_model", "deterministic_pattern");
        }

        GeneratorModelType generatorModelType = parseGeneratorModelType(generatorModelValue);
        setRepeatable(repeatFlowSequence, flowSequenceEnd);
        setSimulationMode(simulationMode);
        setGeneratorModelType(generatorModelType);
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
        flow.scale = parseUnsignedLong(flowConfiguration.getValue(section, "scale", "1"));
        flow.minimum = parseUnsignedLong(flowConfiguration.getValue(section, "min", "0"));
        flow.maximum = parseUnsignedLong(flowConfiguration.getValue(section, "max", "100"));
        flow.duration = parseUnsignedLong(flowConfiguration.getValue(section, "intervalsecs", "300"));
        flow.unixepochstart = parseUnixEpoch(flowConfiguration.getValue(section, "start", "1970/01/01 00:00:00"));
        flow.unixepochend = parseUnixEpoch(flowConfiguration.getValue(section, "end", "1970/01/01 00:00:00"));

        std::string unit = flowConfiguration.getValue(section, "uom", "NA");
        std::strncpy(flow.unitofmeasurement, unit.c_str(), sizeof(flow.unitofmeasurement) - 1);
        flow.unitofmeasurement[sizeof(flow.unitofmeasurement) - 1] = '\0';

        addFlow(flow);
    }

    return true;
}

bool Sensing::removeFlow(Flow flow)
{
    for (std::list<Flow>::iterator it = _flows.begin(); it != _flows.end(); ++it)
    {
        if (it->type == flow.type &&
            it->scale == flow.scale &&
            it->duration == flow.duration &&
            it->unixepochstart == flow.unixepochstart &&
            it->minimum == flow.minimum &&
            it->maximum == flow.maximum &&
            std::string(it->unitofmeasurement) == std::string(flow.unitofmeasurement))
        {
            _flows.erase(it);
            return true;
        }
    }
    return false;
}

bool Sensing::startSimulation(SensorCallback* callback)
{
    _baselineTelemetry.clear(); 
    if (_simulationMode == SimulationModeRealtime)
    {
        std::thread simulationThread(Sensing::SimulationThreadProc, this, callback);
        simulationThread.detach();
    }
    else
    {
        SimulationThreadProc(this, callback);
    }

    return true;
}

void Sensing::SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback)
{
    if (callback == nullptr)
    {
        return;
    }

    size_t flowPos = 0;

    std::ofstream telemetryFile;
    if(sensingInstance->_simulationMode == SimulationModeDataset)
    {
        telemetryFile.open(sensingInstance->_flowSetName);
        if (!telemetryFile.is_open())
        {
            std::cerr << "Failed to open telemetry file for writing: " << sensingInstance->_flowSetName << std::endl;
            return;
        }
        telemetryFile << "Timestamp,Value,Scale,UnitOfMeasurement" << std::endl;
    }

    for (Flow flow : sensingInstance->getFlows())
    {
        // If we are at flow position 1 and flow position 0 was BaseLine type, then we need to generate the statistics
        if(flowPos == 1 && sensingInstance->_flows.front().type == BaseLine)
        {
            sensingInstance->computeMaxOfBaseline();
            sensingInstance->computeMinOfBaseline();
            sensingInstance->computeMedianOfBaseline();
            sensingInstance->computeAverageOfBaseline();

            // You can use these statistics as needed, for example, logging or adjusting subsequent flows
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
        unsigned long stuckValue = sensingInstance->getRandomValueInRange(sensingInstance->_minBaseline, sensingInstance->_maxBaseline);

        // Calculate the total sample count based on time range and duration.
        unsigned long sampleCount = ((flow.unixepochend - flow.unixepochstart) / flow.duration) + 1;
        if (sampleCount == 0)
        {
            sampleCount = 1;
        }

        // Distribution vector to set the elements which need to set with fault values
        // We will set the vector with true using generation model algorithms
        std::vector<bool> distibution(static_cast<size_t>(sampleCount), false);
        std::fill(distibution.begin(), distibution.end(), false);

        switch(sensingInstance->getGeneratorModelType())
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

        for (unsigned long t = flow.unixepochstart, ct = 0; t <= flow.unixepochend; t += flow.duration, ct++)
        {
            Telemetry telemetry;
            telemetry.scale = flow.scale; // Use the flow's scale
            telemetry.unitofmeasurement = std::string(flow.unitofmeasurement); // Use the flow's unit of measurement
            telemetry.timestamp = t; // Use the flow's timestamp in dataset mode
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
                    if (isAnomalySample)
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
                    if (isAnomalySample)
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
                    unsigned long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        unsigned long offsetValue = ((flow.maximum - flow.minimum) / 4) + 1;
                        telemetry.value = baseValue + offsetValue;
                    }
                    else
                    {
                        telemetry.value = baseValue;
                    }
                    break;
                }
                case Noise:
                {
                    unsigned long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        unsigned long jitter = ((flow.maximum - flow.minimum) / 10) + 1;
                        unsigned long direction = static_cast<unsigned long>(rand() % 2);
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
                    unsigned long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        unsigned long maxDrift = ((flow.maximum - flow.minimum) / 3) + 1;
                        unsigned long drift = (ct * maxDrift) / sampleCount;
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
                    unsigned long baseValue = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    if (isAnomalySample)
                    {
                        unsigned long stepWindow = (sampleCount / 5) + 1;
                        unsigned long stepIndex = ct / stepWindow;
                        unsigned long stepSize = ((flow.maximum - flow.minimum) / 8) + 1;
                        telemetry.value = baseValue + (stepIndex * stepSize);
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

            if(sensingInstance->_simulationMode == SimulationModeRealtime)
            {
                std::this_thread::sleep_for(std::chrono::seconds(flow.duration)); // Simulate a delay based on the flow's duration
                callback->onTelemetryReceived(telemetry);
            }
            else
            {
                if(sensingInstance->_simulationMode == SimulationModeDataset)
                {
                    telemetryFile << telemetry.timestamp << "," << telemetry.value << "," << telemetry.scale << "," << telemetry.unitofmeasurement << std::endl;
                }
            }
        }

        flowPos++;
    }

    // Once the flow is complete, check if the mode is dataset, if so, we can call the callback to indicate that the flow is complete
    if(sensingInstance->_simulationMode == SimulationModeDataset)
    {
        telemetryFile.close();
        callback->onTelemetryGenerationComplete(sensingInstance->_flowSetName);
    }
}

void Sensing::computeMaxOfBaseline()
{
    if (_baselineTelemetry.empty())
    {
        _maxBaseline = 0;
        return;
    }

    unsigned long maxValue = 0;
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

    unsigned long minValue = _baselineTelemetry.front().value;
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

    std::list<unsigned long> values;
    for (std::list<Telemetry>::const_iterator it = _baselineTelemetry.begin(); it != _baselineTelemetry.end(); ++it)
    {
        values.push_back(it->value);
    }

    values.sort();

    unsigned long count = static_cast<unsigned long>(values.size());
    unsigned long middle = count / 2;

    if (count % 2 == 0)
    {
        std::list<unsigned long>::const_iterator it = values.begin();
        std::advance(it, middle - 1);
        unsigned long first = *it;

        ++it;
        unsigned long second = *it;

        _medianBaseline = (first + second) / 2;
        return;
    }

    std::list<unsigned long>::const_iterator it = values.begin();
    std::advance(it, middle);
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

    unsigned long total = 0;
    unsigned long count = 0;
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

    _averageBaseline = total / count;
    return;
}

unsigned long Sensing::getRandomValueInRange(unsigned long min, unsigned long max)
{
    if (min >= max)
    {
        return min; // Return min if the range is invalid
    }
    return min + (rand() % (max - min + 1));
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