#pragma once

#include <memory>
#include <QUdpSocket>
#include <QTcpSocket>

#include "Data_Defs.h"
#include "Channel.h"
#include "QTcpSocketAdapter.h"

class Comms : public QObject
{
    Q_OBJECT
public:

    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint32_t Sensor_Address;

    Comms();

    void connectToBaseStation(QHostAddress const& address);

    struct BaseStation
    {
        std::array<uint8_t, 6> mac;
        QHostAddress address;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;
    };

    struct Config
    {
        bool sensorsSleeping = false;
        Clock::duration measurementPeriod;
        Clock::duration commsPeriod;
        Clock::duration computedCommsPeriod;

        //This is computed when creating the config so that this equation holds for any config:
        // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
        //
        //So when creating a new config, this is how to calculate the baseline:
        // m = some measurement (any)
        // config.baseline_time_point = m.time_point - m.index * config.measurement_period
        //
        //The reason for this is to keep the indices valid in all configs
        Clock::time_point baselineTimePoint;
    };

    std::vector<BaseStation> const& getLastBasestations() const;
    std::vector<Sensor> const& getLastSensors() const;
    Config const& getLastConfig() const;

    void process();

public slots:
    void requestConfig();
    void requestSensors();
    void requestBindSensor(std::string const& name);

signals:
    void baseStationDiscovered(BaseStation const& bs);
    void baseStationConnected(BaseStation const& bs);
    void baseStationDisconnected(BaseStation const& bs);
    void configReceived(Config const& config);
    void sensorAdded(Sensor const& sensor);

private slots:
    void broadcastReceived();
    void connectedToBaseStation();
    void disconnectedFromBaseStation();

private:

    void processGetConfigRes();
    void processSetConfigRes();
    void processGetSensorsRes();
    void processAddSensorRes();
    void processRemoveSensorRes();
    void processReportMeasurementReq();
    void processSensorBoundReq();


    QUdpSocket m_broadcastSocket;
    std::vector<BaseStation> m_baseStations;
    std::vector<Sensor> m_sensors;
    Sensor m_sensorWaitingForBinding;
    Config m_config;

    size_t m_connectedBSIndex = size_t(-1);
    QTcpSocketAdapter m_bsSocketAdapter;
    util::comms::Channel<data::Server_Message, QTcpSocketAdapter> m_bsChannel;
};


