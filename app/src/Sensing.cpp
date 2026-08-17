#include "Sensing.hpp"

void SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback); 

Sensing::Sensing()
{
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

bool Sensing::removeFlow(Flow flow)
{
    for (std::list<Flow>::iterator it = _flows.begin(); it != _flows.end(); ++it)
    {
        if (it->type == flow.type &&
            it->value == flow.value &&
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
    _callbackPtr = callback;
    
    // Start the simulation in a separate thread
    std::thread simulationThread(SimulationThreadProc, this, _callbackPtr);
    simulationThread.detach(); // Detach the thread to run independently

    return true;
}

void SimulationThreadProc(Sensing* sensingInstance, SensorCallback* callback)
{
    // Simulate telemetry data generation
    while (true)
    {
        if (callback != nullptr)
        {
            Telemetry telemetry;
            telemetry.timestamp = static_cast<unsigned long>(std::time(nullptr));
            telemetry.value = 100; // Example value
            telemetry.scale = 1; // Example scale
            telemetry.unitofmeasurement = "unit";
            callback->onTelemetryReceived(telemetry);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate a delay between telemetry data points
    }
} 