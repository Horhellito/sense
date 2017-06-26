#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_BaseStationsWidget.h"
#include "Comms.h"

class BaseStationsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BaseStationsWidget(QWidget *parent = 0);

    void init(Comms& comms);

signals:

public slots:
    void baseStationDiscovered(std::array<uint8_t, 6> const& mac, QHostAddress const& address, uint16_t port);

private:
    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    QStandardItemModel m_model;

    struct BaseStation
    {
        std::array<uint8_t, 6> mac;
        QHostAddress address;
        uint16_t port;
    };

    std::vector<BaseStation> m_baseStations;
};


