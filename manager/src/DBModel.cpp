#include "DBModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 8> s_headerNames = {"Sensor", "Index", "Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Errors"};
enum class Column
{
    Sensor,
    Index,
    Timestamp,
    Temperature,
    Humidity,
    Battery,
    Signal,
    Errors
};

static std::array<const char*, 5> s_batteryIconNames = { "battery-0.png", "battery-25.png", "battery-50.png", "battery-75.png", "battery-100.png" };

QIcon getBatteryIcon(float vcc)
{
    constexpr float max = 3.2f;
    constexpr float min = 2.f;
    float percentage = std::max(std::min(vcc, max) - min, 0.f) / (max - min);
    size_t index = std::floor(percentage * s_batteryIconNames.size() + 0.5f);
    return QIcon(QString(":/icons/ui/") + s_batteryIconNames[index]);
}

//////////////////////////////////////////////////////////////////////////

DBModel::DBModel(Comms& comms, DB& db)
    : QAbstractItemModel()
    , m_comms(comms)
    , m_db(db)
{
}

//////////////////////////////////////////////////////////////////////////

DBModel::~DBModel()
{

}

//////////////////////////////////////////////////////////////////////////

QModelIndex DBModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
    {
        return QModelIndex();
    }
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }

//    Tree_Item const* ti = nullptr;
//    if (!parent.isValid())
//    {
//        ti = m_root.get();
//    }
//    else
//    {
//        ti = (Tree_Item*)parent.internalPointer();
//    }

//    if (ti && row < (int)ti->m_children.size())
//    {
//        QModelIndex const& idx = ti->m_children[row]->m_model_index;
//        return createIndex(row, column, idx.internalPointer());
//    }

    return createIndex(row, column, nullptr);

//    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

QModelIndex DBModel::parent(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return QModelIndex();
    }
//    Tree_Item* ti = (Tree_Item*)index.internalPointer();
//    if (!ti)
//    {
//        return QModelIndex();
//    }

//    std::shared_ptr<Tree_Item> parent = ti->m_parent.lock();
//    if (!parent || parent == m_root)
    {
        return QModelIndex();
    }

//    return parent->m_model_index;
}

//////////////////////////////////////////////////////////////////////////

int DBModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return m_measurements.size();
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int DBModel::columnCount(QModelIndex const& index) const
{
    return s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant DBModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant DBModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_measurements.size())
    {
        return QVariant();
    }

    DB::Measurement const& measurement = m_measurements[index.row()];

    Column column = static_cast<Column>(index.column());
    if (role == Qt::DecorationRole)
    {
        if (column == Column::Sensor)
        {
            return QIcon(":/icons/ui/sensor.png");
        }
        else if (column == Column::Battery)
        {
            return getBatteryIcon(measurement.vcc);
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Sensor)
        {
            std::vector<Comms::Sensor> const& sensors = m_comms.getLastSensors();
            auto it = std::find_if(sensors.begin(), sensors.end(), [&measurement](Comms::Sensor const& sensor) { return sensor.id == measurement.sensor_id; });
            if (it == sensors.end())
            {
                return "N/A";
            }
            else
            {
                return it->name.c_str();
            }
        }
        else if (column == Column::Index)
        {
            return measurement.index;
        }
        else if (column == Column::Timestamp)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(measurement.time_point));
            return dt;
        }
        else if (column == Column::Temperature)
        {
            return measurement.temperature;
        }
        else if (column == Column::Humidity)
        {
            return measurement.humidity;
        }
        else if (column == Column::Signal)
        {
            return std::min(measurement.s2b_input_dBm, measurement.b2s_input_dBm);
        }
        else if (column == Column::Errors)
        {
            std::string str;
            if (measurement.flags && DB::Measurement::Flag::COMMS_ERROR)
            {
                str += "Comms";
            }
            if (measurement.flags && DB::Measurement::Flag::SENSOR_ERROR)
            {
                if (!str.empty())
                {
                    str += ", ";
                }
                str += "Sensor";
            }
            if (str.empty())
            {
                str = "-";
            }
            return str.c_str();
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags DBModel::flags(QModelIndex const& index) const
{
    auto defaultFlags = Qt::ItemIsEnabled;

    if (!index.isValid())
    {
        return defaultFlags;
    }

    return defaultFlags;
}

//////////////////////////////////////////////////////////////////////////

void DBModel::setFilter(DB::Filter const& filter)
{
    beginResetModel();
    m_filter = filter;
    m_measurements = m_db.get_filtered_measurements(m_filter);
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
//    else if (!index.isValid() || role == Qt::EditRole)
//    {
//        return false;
//    }
//    Tree_Item* ti = (Tree_Item*)index.internalPointer();
//    if (!ti)
//    {
//        return false;
//    }

//    if (index.column() == 0)
//    {
//        return true;
//    }
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DBModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}
