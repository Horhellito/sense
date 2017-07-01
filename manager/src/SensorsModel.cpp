#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>
#include <QPainter>
#include <QDateTime>
#include <cassert>

static std::array<const char*, 12> s_headerNames = {"Name", "Id", "Address", "Temperature", "Humidity", "Battery", "Signal", "State", "Next Measurement", "Next Comms", "Sensor Errors", "Alarms"};
enum class Column
{
    Name,
    Id,
    Address,
    Temperature,
    Humidity,
    Battery,
    Signal,
    State,
    NextMeasurement,
    NextComms,
    SensorErrors,
    Alarms
};

extern QIcon getBatteryIcon(float vcc);
extern QIcon getSignalIcon(int8_t dBm);

//////////////////////////////////////////////////////////////////////////

SensorsModel::SensorsModel(DB& db)
    : QAbstractItemModel()
    , m_db(db)
{
    QObject::connect(&m_db, &DB::sensorAdded, [this](DB::SensorId id) { sensorAdded(id); });

    size_t sensorCount = m_db.getSensorCount();
    for (size_t i = 0; i < sensorCount; i++)
    {
        sensorAdded(m_db.getSensor(i).id);
    }
}

//////////////////////////////////////////////////////////////////////////

SensorsModel::~SensorsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorAdded(DB::SensorId id)
{
    int32_t sensorIndex = m_db.findSensorIndexById(id);
    if (sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Sensor const& sensor = m_db.getSensor(sensorIndex);

    emit beginInsertRows(QModelIndex(), m_sensors.size(), m_sensors.size());
    SensorData sd;
    sd.sensor = sensor;
    m_sensors.push_back(sd);
    emit endInsertRows();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::bindSensor(std::string const& name)
{

}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::unbindSensor(DB::SensorId id)
{

}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::setShowCheckboxes(bool show)
{
    emit layoutAboutToBeChanged();
    m_showCheckboxes = show;
    emit layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::setSensorChecked(DB::SensorId id, bool checked)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensor.id == id; });
    if (it == m_sensors.end())
    {
        return;
    }
    it->isChecked = checked;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::isSensorChecked(DB::SensorId id) const
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensor.id == id; });
    if (it == m_sensors.end())
    {
        return false;
    }
    return it->isChecked;
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
    {
        return QModelIndex();
    }
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::parent(QModelIndex const& index) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return m_sensors.size();
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::columnCount(QModelIndex const& index) const
{
    return s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
        {
            return s_headerNames[section];
        }
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return QVariant();
    }

    SensorData const& sensorData = m_sensors[index.row()];
    DB::Sensor const& sensor = sensorData.sensor;

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && m_showCheckboxes)
    {
        if (column == Column::Name)
        {
            return sensorData.isChecked ? Qt::Checked : Qt::Unchecked;
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Name)
        {
            return QIcon(":/icons/ui/sensor.png");
        }
        else if (column == Column::Id)
        {
        }
        else if (column == Column::Address)
        {
        }
        else if (column == Column::State)
        {
        }
        else if (column == Column::NextMeasurement)
        {
            if (sensor.nextMeasurementTimePoint.time_since_epoch().count() == 0)
            {
                return QIcon(":/icons/ui/question.png");
            }
        }
        else if (column == Column::NextComms)
        {
            if (sensor.nextCommsTimePoint.time_since_epoch().count() == 0)
            {
                return QIcon(":/icons/ui/question.png");
            }
        }
        else
        {
            if (sensor.isLastMeasurementValid)
            {
                if (column == Column::Battery)
                {
                    return getBatteryIcon(sensor.lastMeasurement.vcc);
                }
                else if (column == Column::Signal)
                {
                    return getSignalIcon(std::min(sensor.lastMeasurement.b2s, sensor.lastMeasurement.s2b));
                }
            }
            else
            {
                if (column == Column::Temperature ||
                    column == Column::Humidity)
                {
                    return QIcon(":/icons/ui/question.png");
                }
            }
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Name)
        {
            return sensor.descriptor.name.c_str();
        }
        else if (column == Column::Id)
        {
            return sensor.id;
        }
        else if (column == Column::Address)
        {
            return sensor.address;
        }
        else if (column == Column::State)
        {
            if (sensor.state == DB::Sensor::State::Active)
            {
                return "Active";
            }
            if (sensor.state == DB::Sensor::State::Sleeping)
            {
                return "Sleeping";
            }
            if (sensor.state == DB::Sensor::State::Unbound)
            {
                return "Not bound";
            }
        }
        else if (column == Column::NextMeasurement)
        {
            if (sensor.nextMeasurementTimePoint.time_since_epoch().count() != 0)
            {
                time_t next = DB::Clock::to_time_t(sensor.nextMeasurementTimePoint);
                QDateTime dt;
                dt.setTime_t(next);
                return dt;
            }
        }
        else if (column == Column::NextComms)
        {
            if (sensor.nextCommsTimePoint.time_since_epoch().count() != 0)
            {
                time_t next = DB::Clock::to_time_t(sensor.nextCommsTimePoint);
                QDateTime dt;
                dt.setTime_t(next);
                return dt;
            }
        }
        else if (column == Column::Alarms)
        {
        }
        else
        {
            if (sensor.isLastMeasurementValid)
            {
                if (column == Column::Temperature)
                {
                    return QString("%1 °C").arg(sensor.lastMeasurement.temperature);
                }
                else if (column == Column::Humidity)
                {
                    return QString("%1 % RH").arg(sensor.lastMeasurement.humidity);
                }
            }
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags SensorsModel::flags(QModelIndex const& index) const
{
    Column column = static_cast<Column>(index.column());

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (m_showCheckboxes && column == Column::Name)
    {
        flags |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    }
    return flags;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    if (!index.isValid())
    {
        return false;
    }

    if (static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return false;
    }

    SensorData& sensorData = m_sensors[index.row()];

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && column == Column::Name)
    {
        sensorData.isChecked = value.toInt() == Qt::Checked;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

constexpr QSize k_iconSize(16, 16);
constexpr int32_t k_iconMargin = 4;

void SensorsModel::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return QStyledItemDelegate::paint(painter, option, index);
    }

    SensorData const& sensorData = m_sensors[index.row()];
    DB::Sensor const& sensor = sensorData.sensor;

    Column column = static_cast<Column>(index.column());
    if (column == Column::Alarms)
    {
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft();

        uint8_t triggeredAlarms = sensor.triggeredAlarms;
        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            static QIcon icon(":/icons/ui/temperature.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            static QIcon icon(":/icons/ui/humidity.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            static QIcon icon(":/icons/ui/battery-0.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
        {
            static QIcon icon(":/icons/ui/sensor.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            static QIcon icon(":/icons/ui/signal-0.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }

        painter->restore();
        return;
    }
    else if (column == Column::SensorErrors)
    {
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft();

        if (sensor.isLastMeasurementValid)
        {
            if (sensor.lastMeasurement.sensorErrors & DB::SensorErrors::CommsError)
            {
                static QIcon icon(":/icons/ui/signal-0.png");
                painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
                pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
            }
            if (sensor.lastMeasurement.sensorErrors & DB::SensorErrors::SensorError)
            {
                static QIcon icon(":/icons/ui/sensor.png");
                painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
                pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
            }
        }

        painter->restore();
        return;
    }

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize SensorsModel::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    SensorData const& sensorData = m_sensors[index.row()];
    DB::Sensor const& sensor = sensorData.sensor;

    Column column = static_cast<Column>(index.column());
    if (column == Column::Alarms)
    {
        int32_t width = 0;

        uint8_t triggeredAlarms = sensor.triggeredAlarms;
        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            width += k_iconSize.width() + k_iconMargin;
        }

        return QSize(width, k_iconSize.height());
    }
    else if (column == Column::SensorErrors)
    {
        int32_t width = 0;

        if (sensor.isLastMeasurementValid)
        {
            if (sensor.lastMeasurement.sensorErrors & DB::SensorErrors::CommsError)
            {
                width += k_iconSize.width() + k_iconMargin;
            }
            if (sensor.lastMeasurement.sensorErrors & DB::SensorErrors::SensorError)
            {
                width += k_iconSize.width() + k_iconMargin;
            }
        }

        return QSize(width, k_iconSize.height());
    }

    return QStyledItemDelegate::sizeHint(option, index);
}

