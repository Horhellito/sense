#include "BaseStationsWidget.h"
#include "Settings.h"
#include <cassert>
#include <QMessageBox>

extern std::string getMacStr(Settings::BaseStationDescriptor::Mac const& mac);

//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);

    m_model.setColumnCount(4);
    m_model.setHorizontalHeaderLabels({"Name", "MAC", "IP", "Status"});

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::Stretch);

    connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);
}

//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::~BaseStationsWidget()
{
    m_ui.list->setModel(nullptr);
    m_comms = nullptr;
    m_unregisteredBaseStations.clear();
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::init(Comms& comms, Settings& settings)
{
    setEnabled(true);

    m_comms = &comms;
    m_settings = &settings;

    connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered);
    connect(m_comms, &Comms::baseStationDisconnected, this, &BaseStationsWidget::baseStationDisconnected);

    for (size_t i = 0; i < settings.getBaseStationCount(); i++)
    {
        Settings::BaseStation const& bs = settings.getBaseStation(i);

        QStandardItem* nameItem = new QStandardItem();
        QStandardItem* macItem = new QStandardItem();
        QStandardItem* ipItem = new QStandardItem();
        QStandardItem* statusItem = new QStandardItem();

        {
            nameItem->setText(bs.descriptor.name.c_str());
            nameItem->setIcon(QIcon(":/icons/ui/station.png"));
            nameItem->setEditable(false);
        }

        {
            Settings::BaseStationDescriptor::Mac mac = bs.descriptor.mac;
            macItem->setText(QString("   %1   ").arg(getMacStr(mac).c_str()));
            macItem->setEditable(false);
        }

        {
            char buf[128];
            sprintf(buf, "  %s  ", bs.descriptor.address.toString().toLatin1().data());
            ipItem->setText(buf);
            ipItem->setEditable(false);
        }

        m_model.appendRow({ nameItem, macItem, ipItem, statusItem });

        bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(i), bs.descriptor.address);
        if (settings.getActiveBaseStationId() == bs.id)
        {
            setStatus(i, connected ? "Active / Connected" : "Active / Disconnected");
        }
        else
        {
            setStatus(i, connected ? "Added / Connected" : "Added / Disconnected");
        }
    }

    setRW();
    connect(&settings, &Settings::userLoggedIn, this, &BaseStationsWidget::setRW);
}


//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setRW()
{
//    m_ui.add->setEnabled(m_settings->isLoggedInAsAdmin());
//    m_ui.remove->setEnabled(m_settings->isLoggedInAsAdmin());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setStatus(int row, std::string const& status)
{
    QStandardItem* statusItem = m_model.item(row, 3);
    statusItem->setText(status.c_str());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::activateBaseStation(QModelIndex const& index)
{
    if (!m_settings->isLoggedInAsAdmin())
    {
        QMessageBox::critical(this, "Error", "You need to be logged in as admin to activate/add base stations.");
        return;
    }

    uint32_t bsIndex = index.row();
    if (bsIndex < m_settings->getBaseStationCount())
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);
        m_settings->setActiveBaseStationId(bs.id);
        bool connected = m_comms->isBaseStationConnected(bs.descriptor.mac);
        setStatus(bsIndex, connected ? "Active / Connected" : "Active / Disconnected");
        return;
    }

    bsIndex -= m_settings->getBaseStationCount();
    if (bsIndex >= m_unregisteredBaseStations.size())
    {
        assert(false);
        return;
    }

    Comms::BaseStationDescriptor const& commsBSDescriptor = m_unregisteredBaseStations[bsIndex];

    Settings::BaseStationDescriptor descriptor;
    descriptor.mac = commsBSDescriptor.mac;
    descriptor.name = commsBSDescriptor.name;
    descriptor.address = commsBSDescriptor.address;
    if (m_settings->addBaseStation(descriptor))
    {
        int32_t index = m_settings->findBaseStationIndexByMac(descriptor.mac);
        if (index >= 0)
        {
            bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(index), commsBSDescriptor.address);
            setStatus(bsIndex, connected ? "Added / Connected" : "Added / Disconnected");
        }
    }

    m_unregisteredBaseStations.erase(m_unregisteredBaseStations.begin() + bsIndex);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDisconnected(Comms::BaseStationDescriptor const& bs)
{
    {
        int32_t bsIndex = m_settings->findBaseStationIndexByMac(bs.mac);
        if (bsIndex >= 0)
        {
            Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);

            if (m_settings->getActiveBaseStationId() == bs.id)
            {
                setStatus(bsIndex, "Active / Disconnected");
            }
            else
            {
                setStatus(bsIndex, "Added / Disconnected");
            }

            return;
        }
    }

    {
        auto it = std::find_if(m_unregisteredBaseStations.begin(), m_unregisteredBaseStations.end(), [&bs](Comms::BaseStationDescriptor const& descriptor) { return descriptor == bs; });
        if (it == m_unregisteredBaseStations.end())
        {
            return;
        }

        size_t index = std::distance(m_unregisteredBaseStations.begin(), it);
        m_model.removeRow(m_settings->getBaseStationCount() + index);

        m_unregisteredBaseStations.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDiscovered(Comms::BaseStationDescriptor const& bs)
{
    int32_t bsIndex = m_settings->findBaseStationIndexByMac(bs.mac);
    if (bsIndex >= 0)
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);

        bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(bsIndex), bs.descriptor.address);
        if (m_settings->getActiveBaseStationId() == bs.id)
        {
            setStatus(bsIndex, connected ? "Active / Connected" : "Active / Disconnected");
        }
        else
        {
            setStatus(bsIndex, connected ? "Added / Connected" : "Added / Disconnected");
        }

        return;
    }

    auto it = std::find_if(m_unregisteredBaseStations.begin(), m_unregisteredBaseStations.end(), [&bs](Comms::BaseStationDescriptor const& descriptor) { return descriptor == bs; });
    if (it != m_unregisteredBaseStations.end())
    {
        return;
    }
    m_unregisteredBaseStations.push_back(bs);

    QStandardItem* nameItem = new QStandardItem();
    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();
    QStandardItem* statusItem = new QStandardItem();

    {
        nameItem->setText(bs.name.c_str());
        nameItem->setIcon(QIcon(":/icons/ui/station.png"));
        nameItem->setEditable(false);
    }

    {
        macItem->setText(QString("   %1   ").arg(getMacStr(bs.mac).c_str()));
        macItem->setEditable(false);
    }

    {
        char buf[128];
        sprintf(buf, "  %s  ", bs.address.toString().toLatin1().data());
        ipItem->setText(buf);
        ipItem->setEditable(false);
    }
    {
        statusItem->setText("Not Added");
    }

    m_model.appendRow({ nameItem, macItem, ipItem, statusItem });
}

//////////////////////////////////////////////////////////////////////////

