#include "aiboxdevice.h"
#include <QFileInfo>
#include <stdio.h>
#include <QLoggingCategory>
#include "setting.h"
#include <QUuid>

Q_LOGGING_CATEGORY(device, "DeviceInfo")

extern const QString kPauseState;
extern const QString kNoPauseState;
const static QString kUserDataPath = "/home/oneai/userdata";
extern const QString kSaveData;
extern const QString kClearData;
const static QString kRecoverPath = "/opt/ota_package/ubtech/recovery.sh";
const static QString kCompletePath = "/opt/ota_package/ubtech/complete.sh";
const static QString kStatePath = "/opt/ota_package/ubtech/state";
extern const QString kAiboxUpdating;
extern const QString kAiboxIdle;


AiboxDevice &AiboxDevice::instance()
{
//    qCDebug(device()) << "enter " << __func__;
    static AiboxDevice s_aiboxDevice;
    return s_aiboxDevice;
}

AiboxDevice::AiboxDevice(QObject *parent) : QObject(parent)
{
    qCDebug(device()) << "enter " << __func__;

    m_eAiboxState = emAiboxState::eAiboxIdle;

    system(kCompletePath.toStdString().data()); // 执行complete.sh脚本生成state状态

    m_pBatteryThread = new BatteryMonitor(this);
    m_chargeType = m_pBatteryThread->getChargeType();
    m_pBatteryThread->start();
    connect(m_pBatteryThread, &BatteryMonitor::chargeChanged, this, &AiboxDevice::onChargeChanged);

    setUuid();
    initAiboxState();

    qDebug() << "battery charge type is " << m_chargeType;
}

AiboxDevice::~AiboxDevice()
{
    m_pBatteryThread->setStop(true);
    m_pBatteryThread->wait(6000);
    delete m_pBatteryThread;
}

void AiboxDevice::initAiboxState()
{
    qCDebug(device()) << "enter " << __func__;

    emResultState eResultState =  getResultState();

    qCDebug(device()) << "getResultState: " << eResultState << ", getAiboxState: " << Setting::instance().getAiboxState();


    if (eResultState == RestoreSuccess)
    {
        // 首先判断上次是否为暂停文件发送状态，如果是aibox切换为“文件发送恢复”状态
        if (Setting::instance().getPauseState() == kPauseState)
        {
            m_eAiboxState =  eAiboxFileSendingResume; // 文件发送恢复

            // 重置状态
            Setting::instance().setPauseState(kNoPauseState);
        }
        else
        {
            m_eAiboxState = eAiboxRestoreSuccess;
        }
    }
    else if (eResultState == UpdateSuccess)
    {
        m_eAiboxState = eAiboxUpdateSuccess;
    }
    else if (eResultState == RestoreImageError)
    {
        m_eAiboxState = eAiboxRestoreFail;
    }
    else if (eResultState == UpdateImageError)
    {
        m_eAiboxState = eAiboxUpdateFail;
    }
    else if (eResultState == Idle)
    {
        if (Setting::instance().getAiboxState() == kAiboxUpdating)
        {
            m_eAiboxState = eAiboxUpdateFail; // 升级失败
            Setting::instance().setAiboxState(kAiboxIdle);
        }
        else
        {
            m_eAiboxState = eAiboxIdle;
        }
    }
}

emAiboxState AiboxDevice::getAiboxState()
{
    return m_eAiboxState;
}

void AiboxDevice::setAiboxState(emAiboxState state)
{
    m_eAiboxState = state;
}

QString AiboxDevice::getSN()
{
//    qCDebug(device()) << "enter " << __func__;
    QString sn = "";
    char buff[64] ={0};
    FILE *fp = popen("/usr/local/UBTTools/ubtsn -r", "r");

    if (fp)
    {
        fread(buff, 1, sizeof(buff), fp);

        sn = buff;

        int pos =sn.indexOf("=");
        if (pos == -1)
        {
            pclose(fp);
            qDebug() << "can not find separator =";
            return "";
        }

        sn = sn.mid(pos + 1);

        // delete all of the last '\n'
        while (sn.endsWith('\n'))
        {
            sn.remove(sn.size() - 1, 1);
        }

        pclose(fp);
    }
    else
    {
        qDebug() << "exec command ubtsn -r fail";
    }

    return sn;
}

QString AiboxDevice::getVersion(const char *path)
{
//    qCDebug(device()) << "enter " << __func__;
    QString version = "";
    FILE *fp = fopen(path, "r");

    if (fp == nullptr)
    {
        qDebug() << "open file " << path << " fail";
    }
    else
    {
        char buff[64] = {0};

        fgets(buff, sizeof(buff), fp);

        version = buff;

        int pos = version.indexOf("_");
        if (pos == -1)
        {
            fclose(fp);
            qDebug() << "can not find separator _";
            return "";
        }

        version = version.mid(pos + 1);

        // delete all of the last '\n'
        while (version.endsWith('\n'))
        {
            version.remove(version.size() - 1, 1);
        }

        fclose(fp);
    }

    return version;
}

QString AiboxDevice::getUuid()
{
    return m_uuid;
}

void AiboxDevice::setUuid()
{
    m_uuid = QUuid::createUuid().toString();
}

emResultState AiboxDevice::getResultState()
{
//    qCDebug(device()) << "enter " << __func__;
    char buff[4] = {0};

    QFile fp(kStatePath);

    if (!fp.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCCritical(device()) << "open " << kStatePath << " fail.";
    }
    else
    {
        fp.read(buff, sizeof(buff));
        fp.close();
    }

    switch (buff[0]) {
    case '0':
        return emResultState::Idle;
    case '1':
        return emResultState::UpdateSuccess;
    case '2':
        return emResultState::RestoreSuccess;
    case '3':
        return emResultState::UpdateImageError;
    case '4':
        return emResultState::RestoreImageError;
    default:
        return emResultState::Idle;
    }
}

emAiboxState AiboxDevice::switchResultStateToAiboxState(const int &state)
{
    emResultState eState = (emResultState)state;
    switch (eState) {
    case emResultState::RestoreSuccess:
        return emAiboxState::eAiboxRestoreSuccess;

    case emResultState::UpdateSuccess:
        return emAiboxState::eAiboxUpdateSuccess;

    case emResultState::RestoreImageError:
        return emAiboxState::eAiboxRestoreFail;

    case emResultState::UpdateImageError:
        return emAiboxState::eAiboxUpdateFail;

    default:
        return emAiboxState::eAiboxIdle;
    }
}

void AiboxDevice::setWakeup()
{
    qCDebug(device()) << "enter " << __func__;
    QString cmd = "xset dpms 0 0 0";
    system(cmd.toStdString().data());

    cmd = "xset s 0";
    system(cmd.toStdString().data());
}

bool AiboxDevice::isSpaceEnough(qint64 size)
{
    qCDebug(device()) << "enter " << __func__;
    const QString cmd = "df / -l --output=avail | tail -1";

    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (!fp)
    {
        qCCritical(device()) << "popen " << cmd << " fail!";
        return false;
    }

    const qint64 needSpace = size / 1024 + (qint64)1024 * 500;

    char result[128] = {0};
    fread(result, 1, sizeof(result), fp);
    pclose(fp);

    QString freeSize = result;

    qint64 freeSpace = freeSize.toLongLong();

    qCDebug(device()) << " free space is " << freeSpace << ", need space is " << needSpace;

    if (freeSpace <= needSpace)
    {
        return false;
    }

    return true;
}

void AiboxDevice::clearUserData()
{
    qCDebug(device()) << "enter " << __func__;
    if (Setting::instance().getUserDataState() == kClearData)
    {
        QString cmd = "rm -rf " + kUserDataPath + "/*";
        system(cmd.toStdString().data());
        Setting::instance().setUserDataState(kSaveData);
    }
    else
    {
        // do nothing
    }
}

void AiboxDevice::startUpdate()
{
    qCDebug(device()) << "enter " << __func__;
    QString update = kRecoverPath + " update";
    qCDebug(device()) << update;
    system(update.toStdString().data());
}

void AiboxDevice::startRestore()
{
    qCDebug(device()) << "enter " << __func__;
    QString restore = kRecoverPath + " restore";
    system(restore.toStdString().data());
}

void AiboxDevice::resetState()
{
    qCDebug(device()) << "enter " << __func__;
    QString cmd = "echo 0 > " + kStatePath;
    system(cmd.toStdString().data());
}

QString AiboxDevice::getChargeType()
{
    return m_chargeType;
}

void AiboxDevice::onChargeChanged(QString type)
{
    qCDebug(device()) << "enter " << __func__;
    qCDebug(device()) << "charge type " << type;

    m_chargeType = type;
}
