#include "DB.h"
#include <algorithm>
#include <cassert>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"

constexpr float k_alertVcc = 2.2f;
constexpr int8_t k_alertSignal = -110;


//////////////////////////////////////////////////////////////////////////

DB::DB()
{

}

//////////////////////////////////////////////////////////////////////////

bool DB::create(std::string const& name)
{
    m_dbFilename = "sense-" + name + ".db";
    m_dataFilename = "sense-" + name + ".data";
    m_mainData.measurements.clear();
    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::open(std::string const& name)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DB::save() const
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DB::saveAs(std::string const& filename) const
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorCount() const
{
    return m_mainData.sensors.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& DB::getSensor(size_t index) const
{
    assert(index < m_mainData.sensors.size());
    return m_mainData.sensors[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::setConfig(ConfigDescriptor const& descriptor)
{
    if (descriptor.name.empty())
    {
        return false;
    }
    if (descriptor.commsPeriod < std::chrono::seconds(1))
    {
        return false;
    }
    if (descriptor.measurementPeriod < std::chrono::seconds(1))
    {
        return false;
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        return false;
    }

    Clock::time_point baselineTimePoint = computeBaselineTimePoint(m_mainData.config, descriptor);

    emit configWillBeChanged();

    m_mainData.config.descriptor = descriptor;
    m_mainData.config.baselineTimePoint = baselineTimePoint;

    emit configChanged();

    return true;
}

//////////////////////////////////////////////////////////////////////////

DB::Config const& DB::getConfig() const
{
    return m_mainData.config;
}

//////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeBaselineTimePoint(Config const& oldconfig, ConfigDescriptor const& newDescriptor)
{
    //This is computed when creating the config so that this equation holds for any config:
    // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
    //
    //So when creating a new config, this is how to calculate the baseline:
    // m = some measurement (any)
    // config.baseline_time_point = m.time_point - m.index * config.measurement_period
    //
    //The reason for this is to keep the indices valid in all configs

    Clock::time_point newBaseline;

    bool found = false;
    for (auto const& pair: m_mainData.measurements)
    {
        if (pair.second.empty())
        {
            continue;
        }

        Measurement m = unpack(pair.first, pair.second.back());
        Clock::time_point tp = m.timePoint - m.index * newDescriptor.measurementPeriod;
        if (!found)
        {
            found = true;
        }
        else
        {
            Clock::duration d = tp - newBaseline;

            //std::abs doesn't work with durations
            if (d < Clock::duration::zero())
            {
                d = -d;
            }

            //check for errors
            if (d > std::chrono::minutes(1))
            {
                assert(false);
            }
        }
        newBaseline = tp;
    }

    return found ? newBaseline : oldconfig.baselineTimePoint;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addSensor(SensorDescriptor const& descriptor)
{
    if (findSensorIndexByName(descriptor.name) >= 0)
    {
        assert(false);
        return false;
    }

    {
        Sensor sensor;
        sensor.descriptor.name = descriptor.name;
        sensor.id = ++m_lastSensorId;
        sensor.state = Sensor::State::Unbound;

        emit sensorWillBeAdded(sensor.id);
        m_mainData.sensors.push_back(sensor);
        emit sensorAdded(sensor.id);
    }

    {
        Sensor& sensor = m_mainData.sensors.back();
        sensor.triggeredAlarms = computeTriggeredAlarm(sensor.id);
        if (sensor.triggeredAlarms != 0)
        {
            emit sensorTriggeredAlarmsChanged(sensor.id);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::bindSensor(SensorDescriptor const& descriptor, SensorAddress address)
{
    int32_t index = findSensorIndexByName(descriptor.name);
    if (index < 0)
    {
        assert(false);
        return false;
    }
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&address](Sensor const& sensor) { return sensor.address == address; });
    if (it != m_mainData.sensors.end())
    {
        assert(false);
        return false;
    }

    Sensor& sensor = m_mainData.sensors[index];

    sensor.address = address;
    sensor.state = Sensor::State::Active;

    emit sensorBound(sensor.id);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    assert(index < m_mainData.sensors.size());

    SensorId id = m_mainData.sensors[index].id;

    {
        auto it = m_mainData.measurements.find(id);
        if (it != m_mainData.measurements.end())
        {
            emit measurementsWillBeRemoved(id);

            for (StoredMeasurement const& sm: it->second)
            {
                PrimaryKey pk = computePrimaryKey(id, sm);
                auto sit = std::lower_bound(m_sortedPrimaryKeys.begin(), m_sortedPrimaryKeys.end(), pk);
                if (sit != m_sortedPrimaryKeys.end() && *sit == pk)
                {
                    m_sortedPrimaryKeys.erase(sit);
                }
            }

            m_mainData.measurements.erase(it);
            emit measurementsRemoved(id);
        }
    }

    emit sensorWillBeRemoved(id);
    m_mainData.sensors.erase(m_mainData.sensors.begin() + index);
    emit sensorRemoved(id);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&name](Sensor const& sensor) { return sensor.descriptor.name == name; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexById(SensorId id) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&id](Sensor const& sensor) { return sensor.id == id; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAlarmCount() const
{
    return m_mainData.alarms.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Alarm const& DB::getAlarm(size_t index) const
{
    assert(index < m_mainData.alarms.size());
    return m_mainData.alarms[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addAlarm(AlarmDescriptor const& descriptor)
{
    if (findAlarmIndexByName(descriptor.name) >= 0)
    {
        assert(false);
        return false;
    }
    Alarm alarm;
    alarm.descriptor = descriptor;
    alarm.id = ++m_lastAlarmId;

    emit alarmWillBeAdded(alarm.id);
    m_mainData.alarms.push_back(alarm);
    emit alarmAdded(alarm.id);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeAlarm(size_t index)
{
    assert(index < m_mainData.alarms.size());
    AlarmId id = m_mainData.alarms[index].id;

    emit alarmWillBeRemoved(id);
    m_mainData.alarms.erase(m_mainData.alarms.begin() + index);
    emit alarmRemoved(id);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findAlarmIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.alarms.begin(), m_mainData.alarms.end(), [&name](Alarm const& alarm) { return alarm.descriptor.name == name; });
    if (it == m_mainData.alarms.end())
    {
        return -1;
    }
    return std::distance(m_mainData.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeTriggeredAlarm(Measurement const& measurement) const
{
    uint8_t triggered = 0;

    for (Alarm const& alarm: m_mainData.alarms)
    {
        AlarmDescriptor const& descriptor = alarm.descriptor;
        if (descriptor.highTemperatureWatch && measurement.temperature > descriptor.highTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }
        if (descriptor.lowTemperatureWatch && measurement.temperature < descriptor.lowTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }

        if (descriptor.highHumidityWatch && measurement.humidity > descriptor.highHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }
        if (descriptor.lowHumidityWatch && measurement.humidity < descriptor.lowHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }

        if (descriptor.lowVccWatch && measurement.vcc <= k_alertVcc)
        {
            triggered |= TriggeredAlarm::LowVcc;
        }
        if (descriptor.sensorErrorsWatch && measurement.sensorErrors != 0)
        {
            triggered |= TriggeredAlarm::SensorErrors;
        }
        if (descriptor.lowSignalWatch && std::min(measurement.s2b, measurement.b2s) <= k_alertSignal)
        {
            triggered |= TriggeredAlarm::LowSignal;
        }
    }

    return triggered;
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeTriggeredAlarm(SensorId sensorId) const
{
    uint8_t triggered = 0;

    auto it = m_mainData.measurements.find(sensorId);
    if (it == m_mainData.measurements.end())
    {
        return triggered;
    }

    for (StoredMeasurement const& sm: it->second)
    {
        Measurement m = unpack(sensorId, sm);
        triggered |= computeTriggeredAlarm(m);
        if ((triggered & TriggeredAlarm::All) == TriggeredAlarm::All)
        {
            return triggered;
        }
    }

    return triggered;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(Measurement const& measurement)
{
    int32_t sensorIndex = findSensorIndexById(measurement.sensorId);
    if (sensorIndex < 0)
    {
        assert(false);
        return false;
    }

    PrimaryKey pk = computePrimaryKey(measurement);

    //check for duplicates
    auto it = std::lower_bound(m_sortedPrimaryKeys.begin(), m_sortedPrimaryKeys.end(), pk);
    if (it != m_sortedPrimaryKeys.end() && *it == pk)
    {
        assert(false);
        return false;
    }

    emit measurementsWillBeAdded(measurement.sensorId);
    m_mainData.measurements[measurement.sensorId].push_back(pack(measurement));
    m_sortedPrimaryKeys.insert(it, pk);
    emit measurementsAdded(measurement.sensorId);

    Sensor& sensor = m_mainData.sensors[sensorIndex];
    sensor.isLastMeasurementValid = true;
    sensor.lastMeasurement = measurement;

    emit sensorMeasurementChanged(sensor.id);

    uint8_t triggeredAlarms = sensor.triggeredAlarms;
    sensor.triggeredAlarms |= computeTriggeredAlarm(measurement);
    if (sensor.triggeredAlarms != triggeredAlarms)
    {
        emit sensorTriggeredAlarmsChanged(sensor.id);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getAllMeasurements() const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_mainData.measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
        {
            result.push_back(unpack(pair.first, sm));
        }
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAllMeasurementCount() const
{
    size_t count = 0;
    for (auto const& pair : m_mainData.measurements)
    {
        count += pair.second.size();
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getFilteredMeasurements(Filter const& filter) const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_mainData.measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
        {
            Measurement m = unpack(pair.first, sm);
            if (cull(m, filter))
            {
                result.push_back(m);
            }
        }
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    size_t count = 0;
    for (auto const& pair : m_mainData.measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
        {
            Measurement m = unpack(pair.first, sm);
            if (cull(m, filter))
            {
                count++;
            }
        }
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////

bool DB::getLastMeasurementForSensor(SensorId sensor_id, Measurement& measurement) const
{
    auto it = m_mainData.measurements.find(sensor_id);
    if (it == m_mainData.measurements.end())
    {
        assert(false);
        return false;
    }

    Clock::time_point best_time_point = Clock::time_point(Clock::duration::zero());
    for (StoredMeasurement const& sm: it->second)
    {
        Measurement m = unpack(sensor_id, sm);
        if (m.timePoint > best_time_point)
        {
            measurement = m;
            best_time_point = m.timePoint;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::cull(Measurement const& measurement, Filter const& filter) const
{
    if (filter.useSensorFilter)
    {
        if (std::find(filter.sensorIds.begin(), filter.sensorIds.end(), measurement.sensorId) == filter.sensorIds.end())
        {
            return false;
        }
    }
    if (filter.useIndexFilter)
    {
        if (measurement.index < filter.indexFilter.min || measurement.index > filter.indexFilter.max)
        {
            return false;
        }
    }
    if (filter.useTimePointFilter)
    {
        if (measurement.timePoint < filter.timePointFilter.min || measurement.timePoint > filter.timePointFilter.max)
        {
            return false;
        }
    }
    if (filter.useTemperatureFilter)
    {
        if (measurement.temperature < filter.temperatureFilter.min || measurement.temperature > filter.temperatureFilter.max)
        {
            return false;
        }
    }
    if (filter.useHumidityFilter)
    {
        if (measurement.humidity < filter.humidityFilter.min || measurement.humidity > filter.humidityFilter.max)
        {
            return false;
        }
    }
    if (filter.useVccFilter)
    {
        if (measurement.vcc < filter.vccFilter.min || measurement.vcc > filter.vccFilter.max)
        {
            return false;
        }
    }
    if (filter.useB2SFilter)
    {
        if (measurement.b2s < filter.b2sFilter.min || measurement.b2s > filter.b2sFilter.max)
        {
            return false;
        }
    }
    if (filter.useS2BFilter)
    {
        if (measurement.s2b < filter.s2bFilter.min || measurement.s2b > filter.s2bFilter.max)
        {
            return false;
        }
    }
    if (filter.useSensorErrorsFilter)
    {
        bool has_errors = measurement.sensorErrors != 0;
        if (has_errors != filter.sensorErrorsFilter)
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

inline DB::StoredMeasurement DB::pack(Measurement const& m)
{
    StoredMeasurement sm;
    sm.index = m.index;
    sm.time_point = Clock::to_time_t(m.timePoint);
    sm.temperature = static_cast<int16_t>(std::max(std::min(m.temperature, 320.f), -320.f) * 100.f);
    sm.humidity = static_cast<int16_t>(std::max(std::min(m.humidity, 320.f), -320.f) * 100.f);
    sm.vcc = static_cast<uint8_t>(std::max(std::min((m.vcc - 2.f), 2.55f), 0.f) * 100.f);
    sm.b2s = m.b2s;
    sm.s2b = m.s2b;
    sm.flags = m.sensorErrors;
    return sm;
}

//////////////////////////////////////////////////////////////////////////

inline DB::Measurement DB::unpack(SensorId sensor_id, StoredMeasurement const& sm)
{
    Measurement m;
    m.sensorId = sensor_id;
    m.index = sm.index;
    m.timePoint = Clock::from_time_t(sm.time_point);
    m.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.b2s = sm.b2s;
    m.s2b = sm.s2b;
    m.sensorErrors = sm.flags;
    return m;
}

//////////////////////////////////////////////////////////////////////////

inline DB::PrimaryKey DB::computePrimaryKey(Measurement const& m)
{
    return (PrimaryKey(m.sensorId) << 32) | PrimaryKey(m.index);
}

//////////////////////////////////////////////////////////////////////////

inline DB::PrimaryKey DB::computePrimaryKey(SensorId sensor_id, StoredMeasurement const& m)
{
    return (PrimaryKey(sensor_id) << 32) | PrimaryKey(m.index);
}

//////////////////////////////////////////////////////////////////////////
