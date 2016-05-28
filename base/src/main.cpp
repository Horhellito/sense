#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <boost/thread.hpp>
#include <pigpio.h>

#include "CRC.h"
#include "DB.h"
#include "Comms.h"
#include "Storage.h"
#include "Sensors.h"
#include "Scheduler.h"

#include "DB.h"

#include "rfm22b/rfm22b.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

static Comms s_comms;
static Sensors s_sensors;

typedef std::chrono::high_resolution_clock Clock;

extern void run_tests();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


////////////////////////////////////////////////////////////////////////

void fill_config_packet(data::Config& packet, Sensors::Sensor const& sensor)
{
    packet.base_time_point = chrono::time_s(Clock::to_time_t(Clock::now()));
    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_measurement_period()).count());
    packet.next_measurement_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_measurement_time_point()));
    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count());
    packet.next_comms_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_comms_time_point(sensor.id)));
    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(sensor.id);

    std::cout << "\tConfig for id: " << sensor.id
                                            << "\n\tbase time point: " << packet.base_time_point.ticks
                                            << "\n\tmeasurement period: " << packet.measurement_period.count
                                            << "\n\tnext measurement time point: " << packet.next_measurement_time_point.ticks
                                            << "\n\tcomms period: " << packet.comms_period.count
                                            << "\n\tnext comms time point: " << packet.next_comms_time_point.ticks
                                            << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
                                            << "\n";
}

////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    size_t tries = 0;

    gpioInitialise();

//    run_tests();

    //s_sensors.load_sensors("sensors.config");

    static const std::string db_server = "192.168.1.205";
    static const std::string db_db = "sensor-test";
    static const std::string db_username = "admin";
    static const std::string db_password = "admin";
    static const uint16_t port = 0;

    if (!s_sensors.init(db_server, db_db, db_username, db_password, port))
    {
        std::cerr << "Sensors init failed\n";
        return -1;
    }

    tries = 0;
    while (!s_comms.init(1))
    {
        tries++;
        std::cerr << "comms init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    s_comms.set_check_address(Comms::BROADCAST_ADDRESS);

    while (true)
    {
        //std::string cmd = read_fifo();
        //process_command(cmd);
        //client.process();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        uint8_t size = s_comms.receive_packet(1000);
        if (size > 0)
        {
            data::Type type = s_comms.get_packet_type();
            //LOG_LN("Received packed of " << (int)size << " bytes. Type: "<< (int)type);
            if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
            {
                Sensors::Sensor const* sensor = s_sensors.add_expected_sensor();
                if (sensor)
                {
                    std::cout << "Adding sensor " << sensor->name << ", id " << sensor->id << ", address " << sensor->address << "\n";

                    data::Pair_Response packet;
                    packet.address = sensor->address;
                    s_comms.set_address(Comms::BASE_ADDRESS);
                    s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(10))
                    {
                        std::cout << "Pair successful\n";
                    }
                    else
                    {
                        s_sensors.revert_to_expected_sensor(sensor->id);
                        std::cerr << "Pair failed (comms)\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "Pairing failed (db)\n";
                }
            }
            else if (type == data::Type::MEASUREMENT_BATCH && size >= data::MEASUREMENT_BATCH_PACKET_MIN_SIZE)
            {
                Sensors::Sensor_Address address = s_comms.get_packet_address();
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "Measurement batch reported by " << sensor->id << "\n";

                    data::Measurement_Batch batch;
                    memcpy(&batch, s_comms.get_packet_payload(), size);
                    if (size == data::MEASUREMENT_BATCH_PACKET_MIN_SIZE + batch.count * sizeof(data::Measurement))
                    {
                        std::cout << "\tIndices: " << batch.index << " - " << batch.index + batch.count << "\n";

                        Sensors::Measurement m;
                        size_t count = std::min<size_t>(batch.count, data::Measurement_Batch::MAX_COUNT);
                        for (size_t i = 0; i < count; i++)
                        {
                            m.index = batch.index + i;
                            m.flags = batch.measurements[i].flags;
                            m.s2b_input_dBm = s_comms.get_input_dBm();
                            batch.measurements[i].unpack(m.vcc, m.humidity, m.temperature);

                            s_sensors.add_measurement(sensor->id, m);
                        }
                    }
                    else
                    {
                        std::cerr << "\tMalformed measurement batch data!\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::FIRST_CONFIG_REQUEST && size == sizeof(data::First_Config_Request))
            {
                Sensors::Sensor_Address address = s_comms.get_packet_address();
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "First config requested by " << sensor->id << "\n";

                    data::First_Config packet;
                    packet.first_measurement_index = s_sensors.compute_next_measurement_index();
                    s_sensors.set_sensor_measurement_range(sensor->id, packet.first_measurement_index, 0);
                    fill_config_packet(packet.config, *sensor);

                    s_comms.set_address(address);
                    s_comms.begin_packet(data::Type::FIRST_CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(3))
                    {
                        std::cout << "\tFirst Config successful\n";
                    }
                    else
                    {
                        std::cerr << "\tFirst Config failed\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::CONFIG_REQUEST && size == sizeof(data::Config_Request))
            {
                data::Config_Request const& config_request = *reinterpret_cast<data::Config_Request const*>(s_comms.get_packet_payload());

                Sensors::Sensor_Address address = s_comms.get_packet_address();
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "Config requested by " << sensor->id << "\n";
                    std::cout << "\tStored range: " << config_request.first_measurement_index << " to " << config_request.first_measurement_index + config_request.measurement_count << " (" << config_request.measurement_count << " measurements) \n";

                    s_sensors.set_sensor_measurement_range(sensor->id, config_request.first_measurement_index, config_request.measurement_count);
                    s_sensors.set_sensor_b2s_input_dBm(sensor->id, config_request.b2s_input_dBm);

                    data::Config packet;
                    fill_config_packet(packet, *sensor);

                    s_comms.set_address(address);
                    s_comms.begin_packet(data::Type::CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(5))
                    {
                        std::cout << "\tSchedule successful\n";
                    }
                    else
                    {
                        std::cerr << "\tSchedule failed\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
        }
        std::cout << std::flush;
    }

    return 0;
}

