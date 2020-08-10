#include "setting.h"
#include <QDir>

namespace {
QString CONFIG_FILE = QDir::homePath() + "/.config/ota.conf";
QString CONFIG_OTA_GROUP{"ota_info"};
QString OTA_VERSION{"ota_version"};
QString PAUSE_STATE{"pause_state"};
QString USER_DATA_STATE{"user_data_state"};
}

Setting::Setting(const QString &fileName, QSettings::Format format, QObject *parent)
    : QSettings(fileName, format, parent)
{
}

Setting& Setting::instance()
{
    static Setting s_singleton(CONFIG_FILE, QSettings::NativeFormat);
    return s_singleton;
}


QString Setting::getVersion()
{
    if (contains(OTA_VERSION))
    {
        return value(OTA_VERSION).toString();
    }
    else
    {
        return "";
    }
}

void Setting::setVersion(const QString &version)
{
    setValue(OTA_VERSION, version);
    sync();
}

QString Setting::getPauseState()
{
    if (contains(PAUSE_STATE))
    {
        return value(PAUSE_STATE).toString();
    }
    else
    {
        return "";
    }

}

void Setting::setPauseState(const QString &state)
{
    setValue(PAUSE_STATE, state);
    sync();
}

QString Setting::getUserDataState()
{
    if (contains(USER_DATA_STATE))
    {
        return value(USER_DATA_STATE).toString();
    }
    else
    {
        return "";
    }

}

void Setting::setUserDataState(const QString &state)
{
    setValue(USER_DATA_STATE, state);
    sync();
}


