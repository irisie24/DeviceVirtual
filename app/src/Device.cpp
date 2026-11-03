#include "Device.hpp"
#include "Configuration.hpp"

#include <sstream>
#include <ctime>
#include <iomanip>

unsigned long parseDeviceUnsignedLong(const std::string& value)
{
    std::stringstream stream(value);
    unsigned long result = 0;
    stream >> result;
    return result;
}

long parseDeviceLong(const std::string& value)
{
    std::stringstream stream(value);
    long result = 0;
    stream >> result;
    return result;
}

unsigned long parseDeviceUnixEpoch(const std::string& value)
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

SimulationMode parseDeviceSimulationMode(const std::string& value)
{
    if (value == "realtime")
    {
        return SimulationModeRealtime;
    }

    return SimulationModeDataset;
}

GeneratorModelType parseDeviceGeneratorModelType(const std::string& value)
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

std::string getConfigValueWithFallback(Configuration& configuration,
                                       const std::string& primarySection,
                                       const std::string& key,
                                       const std::string& defaultValue)
{
    std::string fallbackValue = defaultValue;
    if (configuration.isSection("General"))
    {
        fallbackValue = configuration.getValue("General", key, defaultValue);
    }

    if (configuration.isSection(primarySection))
    {
        return configuration.getValue(primarySection, key, fallbackValue);
    }

    return fallbackValue;
}

Device::Device()
{
    _deviceId = "UnknownDevice";
    _deviceName = "";
    _make = "";
    _model = "";
    _sensorOperatingMax = 0;
    _sensorOperatingMin = 0;
    _sensorGeoLat = 0;
    _sensorGeoLon = 0;
    _sensorGeoAlt = 0;
    _scale = 0;
    _unitOfMeasurement = "";
}

Device::~Device()
{
}

bool Device::loadDeviceDefinition(Configuration& deviceConfiguration)
{
    bool hasLegacyGeneralSection = deviceConfiguration.isSection("General");
    
    if (!hasLegacyGeneralSection)
    {
        return false;
    }

    setDeviceId(getConfigValueWithFallback(deviceConfiguration, "General", "device_id", "UnknownDevice"));
    setDeviceName(getConfigValueWithFallback(deviceConfiguration, "General", "device_name", ""));
    setMake(getConfigValueWithFallback(deviceConfiguration, "General", "make", ""));
    setModel(getConfigValueWithFallback(deviceConfiguration, "General", "model", ""));
    setSensorOperatingMax(parseDeviceLong(getConfigValueWithFallback(deviceConfiguration, "General", "sensor_operating_max", "0")));
    setSensorOperatingMin(parseDeviceLong(getConfigValueWithFallback(deviceConfiguration, "General", "sensor_operating_min", "0")));
    setSensorGeoLat(parseDeviceLong(getConfigValueWithFallback(deviceConfiguration, "General", "sensor_geo_lat", "0")));
    setSensorGeoLon(parseDeviceLong(getConfigValueWithFallback(deviceConfiguration, "General", "sensor_geo_lon", "0")));
    setSensorGeoAlt(parseDeviceLong(getConfigValueWithFallback(deviceConfiguration, "General", "sensor_geo_alt", "0")));
    setScale(parseDeviceUnsignedLong(getConfigValueWithFallback(deviceConfiguration, "General", "scale", "0")));
    setUnitOfMeasurement(getConfigValueWithFallback(deviceConfiguration, "General", "unit_of_measurement", ""));

    return true;
}

bool Device::loadSensingFlowDefinition(Configuration& flowConfiguration)
{
    _sensing.setDeviceId(_deviceId);
    return _sensing.loadSensorDefinition(flowConfiguration);
}

bool Device::loadActuationRuleDefinition(Configuration& ruleConfiguration)
{
    _actuation.setDeviceId(_deviceId);
    return _actuation.loadRuleDefinition(ruleConfiguration);
}

void Device::setDeviceId(const std::string& deviceId)
{
    _deviceId = deviceId;
    _sensing.setDeviceId(_deviceId);
    _actuation.setDeviceId(_deviceId);
}

std::string Device::getDeviceId() const
{
    return _deviceId;
}

void Device::setDeviceName(const std::string& deviceName)
{
    _deviceName = deviceName;
}

std::string Device::getDeviceName() const
{
    return _deviceName;
}

void Device::setMake(const std::string& make)
{
    _make = make;
}

std::string Device::getMake() const
{
    return _make;
}

void Device::setModel(const std::string& model)
{
    _model = model;
}

std::string Device::getModel() const
{
    return _model;
}

SimulationMode Device::getSimulationMode() const
{
    return _sensing.getSimulationMode();
}

void Device::setSensorOperatingMax(long sensorOperatingMax)
{
    _sensorOperatingMax = sensorOperatingMax;
    SensingRuntimeConfiguration runtimeConfiguration = _sensing.getRuntimeConfiguration();
    runtimeConfiguration.sensorOperatingMax = _sensorOperatingMax;
    _sensing.setRuntimeConfiguration(runtimeConfiguration);
}

long Device::getSensorOperatingMax() const
{
    return _sensorOperatingMax;
}

void Device::setSensorOperatingMin(long sensorOperatingMin)
{
    _sensorOperatingMin = sensorOperatingMin;
    SensingRuntimeConfiguration runtimeConfiguration = _sensing.getRuntimeConfiguration();
    runtimeConfiguration.sensorOperatingMin = _sensorOperatingMin;
    _sensing.setRuntimeConfiguration(runtimeConfiguration);
}

long Device::getSensorOperatingMin() const
{
    return _sensorOperatingMin;
}

void Device::setSensorGeoLat(long sensorGeoLat)
{
    _sensorGeoLat = sensorGeoLat;
}

long Device::getSensorGeoLat() const
{
    return _sensorGeoLat;
}

void Device::setSensorGeoLon(long sensorGeoLon)
{
    _sensorGeoLon = sensorGeoLon;
}

long Device::getSensorGeoLon() const
{
    return _sensorGeoLon;
}

void Device::setSensorGeoAlt(long sensorGeoAlt)
{
    _sensorGeoAlt = sensorGeoAlt;
}

long Device::getSensorGeoAlt() const
{
    return _sensorGeoAlt;
}

void Device::setScale(unsigned long scale)
{
    _scale = scale;
    SensingRuntimeConfiguration runtimeConfiguration = _sensing.getRuntimeConfiguration();
    runtimeConfiguration.telemetryScale = _scale;
    _sensing.setRuntimeConfiguration(runtimeConfiguration);
}

unsigned long Device::getScale() const
{
    return _scale;
}

void Device::setUnitOfMeasurement(const std::string& unitOfMeasurement)
{
    _unitOfMeasurement = unitOfMeasurement;
    SensingRuntimeConfiguration runtimeConfiguration = _sensing.getRuntimeConfiguration();
    runtimeConfiguration.telemetryUnitOfMeasurement = _unitOfMeasurement;
    _sensing.setRuntimeConfiguration(runtimeConfiguration);
}

std::string Device::getUnitOfMeasurement() const
{
    return _unitOfMeasurement;
}

bool Device::startSensingSimulation(SensorCallback* callback)
{
    _sensing.setDeviceId(_deviceId);
    SensingRuntimeConfiguration runtimeConfiguration = _sensing.getRuntimeConfiguration();
    runtimeConfiguration.sensorOperatingMin = _sensorOperatingMin;
    runtimeConfiguration.sensorOperatingMax = _sensorOperatingMax;
    runtimeConfiguration.telemetryScale = _scale;
    runtimeConfiguration.telemetryUnitOfMeasurement = _unitOfMeasurement;
    _sensing.setRuntimeConfiguration(runtimeConfiguration);
    _sensing.normalizeFlowRanges(runtimeConfiguration);
    return _sensing.startSimulation(_sensing.getRuntimeConfiguration(), callback);
}

Sensing& Device::getSensing()
{
    return _sensing;
}

const Sensing& Device::getSensing() const
{
    return _sensing;
}

Actuation& Device::getActuation()
{
    return _actuation;
}

const Actuation& Device::getActuation() const
{
    return _actuation;
}
