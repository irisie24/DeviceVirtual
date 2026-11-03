#ifndef DEVICE_
#define DEVICE_

#include <string>
#include <filesystem>
#include "Sensing.hpp"
#include "Actuation.hpp"
#include "Configuration.hpp"

class Device
{
public:
    Device();
    ~Device();

    bool loadDeviceDefinition(Configuration& deviceConfiguration);
    bool loadSensingFlowDefinition(Configuration& flowConfiguration);
    bool loadActuationRuleDefinition(Configuration& ruleConfiguration);

    void setDeviceId(const std::string& deviceId);
    std::string getDeviceId() const;
    SimulationMode getSimulationMode() const;

    void setDeviceName(const std::string& deviceName);
    std::string getDeviceName() const;

    void setMake(const std::string& make);
    std::string getMake() const;

    void setModel(const std::string& model);
    std::string getModel() const;

    void setSensorOperatingMax(long sensorOperatingMax);
    long getSensorOperatingMax() const;

    void setSensorOperatingMin(long sensorOperatingMin);
    long getSensorOperatingMin() const;

    void setSensorGeoLat(long sensorGeoLat);
    long getSensorGeoLat() const;

    void setSensorGeoLon(long sensorGeoLon);
    long getSensorGeoLon() const;

    void setSensorGeoAlt(long sensorGeoAlt);
    long getSensorGeoAlt() const;

    void setScale(unsigned long scale);
    unsigned long getScale() const;

    void setUnitOfMeasurement(const std::string& unitOfMeasurement);
    std::string getUnitOfMeasurement() const;

    bool startSensingSimulation(SensorCallback* callback);

    Sensing& getSensing();
    const Sensing& getSensing() const;

    Actuation& getActuation();
    const Actuation& getActuation() const;

private:
    std::string _deviceId;
    std::string _deviceName;
    std::string _make;
    std::string _model;
    long _sensorOperatingMax;
    long _sensorOperatingMin;
    long _sensorGeoLat;
    long _sensorGeoLon;
    long _sensorGeoAlt;
    unsigned long _scale;
    std::string _unitOfMeasurement;
    Sensing _sensing;
    Actuation _actuation;
};

#endif
