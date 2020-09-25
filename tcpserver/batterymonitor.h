#ifndef BATTERYMONITOR_H
#define BATTERYMONITOR_H

#include <QObject>
#include <QThread>
#include <QFile>

class BatteryMonitor : public QThread
{
    Q_OBJECT
public:
    explicit BatteryMonitor(QObject *parent = nullptr);
    void setStop(bool isStop);
    QString getChargeType();

protected:
    void run();

signals:
    void chargeChanged(QString state);

public slots:

private:
    QFile m_file;
    bool m_bStop;
    QString m_previous;
    QString m_current;
};

#endif // BATTERYMONITOR_H
