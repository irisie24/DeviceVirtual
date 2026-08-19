#include "Sensing.hpp"

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

        // Calculate the number of iterations based on the flow's duration and the time range
        unsigned long iterations = 1;
        if (flow.duration > 0)
        {
            iterations = (flow.unixepochend - flow.unixepochstart) / flow.duration;
            if (iterations == 0)
            {
                iterations = 1;
            }
        }

        unsigned long startTime = flow.unixepochstart;

        for (unsigned long t = flow.unixepochstart, ct = 0; t <= flow.unixepochend; t += flow.duration, ct++)
        {
            Telemetry telemetry;
            telemetry.scale = flow.scale; // Use the flow's scale
            telemetry.unitofmeasurement = std::string(flow.unitofmeasurement); // Use the flow's unit of measurement
            telemetry.timestamp = t; // Use the flow's timestamp in dataset mode
            
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
                    // If the count ct % iterations is 0, we generate a spike value, otherwise we generate a normal value
                    if (ct % iterations == 0)
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
                    telemetry.value = 0; // Simulate a dead flow with value 0
                    break;
                }
                case Stuck:
                {
                    telemetry.value = stuckValue; // Simulate a stuck flow with the same value
                    break;
                }
                case Offset:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    break;
                }
                case Noise:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    break;
                }
                case DriftLinear:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    break;
                }
                case DriftStepped:
                {
                    telemetry.value = sensingInstance->getRandomValueInRange(flow.minimum, flow.maximum);
                    break;
                }
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
        }

        flowPos++;
    }

    // Once the flow is complete, check if the mode is dataset, if so, we can call the callback to indicate that the flow is complete
    if(sensingInstance->_simulationMode == SimulationModeDataset)
    {
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