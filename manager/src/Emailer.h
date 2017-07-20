#pragma once

#include <string>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <QTimer>

#include "Settings.h"
#include "DB.h"


class Emailer : public QObject
{
    Q_OBJECT
public:
    Emailer();
    ~Emailer();

    void init(Settings& settings, DB& db);
    void shutdown();

    void sendReportEmail(DB::Report const& report);

private slots:
    void alarmTriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md);
    void alarmUntriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md);

private:
    void checkReports();

    Settings* m_settings = nullptr;
    DB* m_db = nullptr;
    QTimer m_timer;

    struct Email
    {
        Settings::EmailSettings settings;
        std::string to;
        std::string subject;
        std::string body;
        std::vector<std::string> files;
    };

    std::atomic_bool m_threadsExit = { false };

    void sendAlarmTriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendAlarmUntriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendAlarmEmail(Email& email, DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendEmail(Email const& email);
    void emailThreadProc();
    static void sendEmails(std::vector<Email> const& emails);
    std::atomic_bool m_emailThreadTriggered = { false };
    std::thread m_emailThread;
    std::condition_variable m_emailCV;
    std::mutex m_emailMutex;
    std::vector<Email> m_emails;
};
