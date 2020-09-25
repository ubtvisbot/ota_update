#include "batterymonitor.h"
#include <QDebug>

const static QString kBatteryFile = "/sys/class/power_supply/battery/charger_type";

BatteryMonitor::BatteryMonitor(QObject *parent) : QThread(parent)
{
    m_file.setFileName(kBatteryFile);
    m_bStop = false;

    if (m_file.open(QIODevice::ReadOnly))
    {
        char data[64] = {0};
        m_file.read(data, sizeof(data));
        m_previous = m_current = data;
        m_file.close();
    }
    else
    {
        qDebug() << "BatteryMonitor: open battery file fail";
    }
}

void BatteryMonitor::setStop(bool isStop)
{
    m_bStop = isStop;
}

QString BatteryMonitor::getChargeType()
{
    return m_current;
}

void BatteryMonitor::run()
{
    qDebug() << __func__;
    while (!m_bStop)
    {
        sleep(5);

        if (m_file.open(QIODevice::ReadOnly))
        {
            char data[64] = {0};
            m_file.read(data, sizeof(data));
            m_current = data;

            if (m_current != m_previous)
            {
                m_previous = m_current;
                emit chargeChanged(m_current);
            }
            m_file.close();
        }
        else
        {
            qDebug() << "run : open battery file fail";
        }
    }
}
