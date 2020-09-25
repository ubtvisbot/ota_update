#ifndef AIBOXDEVICE_H
#define AIBOXDEVICE_H

#include <QObject>
#include <QString>
#include "aiboxstate.h"
#include "batterymonitor.h"

class AiboxDevice : public QObject
{
    Q_OBJECT
public:
    static AiboxDevice& instance();
    ~AiboxDevice();

    void initAiboxState();
    emAiboxState getAiboxState();
    void setAiboxState(emAiboxState state);

    QString getSN();
    QString getVersion(const char *path);

    QString getUuid();
    void setUuid();

    /*
     * 获取上次升级或者恢复后recovery系统给出的状态
     */
    emResultState getResultState();

    /*
     * 把从 recovery系统返回的状态转换为客户端所需要的状态
     */
    emAiboxState switchResultStateToAiboxState(const int &state);

    /*
     * 设置电脑不休眠
     */
    void setWakeup();
    bool isSpaceEnough(qint64 size);
    void clearUserData();

    void startUpdate();
    void startRestore();
    void resetState();

    QString getChargeType();

public slots:
    void onChargeChanged(QString type);

protected:
    AiboxDevice(QObject *parent = nullptr);
    Q_DISABLE_COPY(AiboxDevice)


private:
    BatteryMonitor* m_pBatteryThread;
    QString m_chargeType;
    emAiboxState m_eAiboxState;
    QString m_uuid;
};

#endif // AIBOXDEVICE_H
