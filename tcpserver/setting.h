#ifndef SETTING_H
#define SETTING_H

#include <QSettings>

class Setting : public QSettings
{
    Q_OBJECT
public:

    static Setting& instance();

    void init();

    QString getVersion();
    void setVersion(const QString &version);

    QString getPauseState();
    void setPauseState(const QString &state);

    QString getUserDataState();
    void setUserDataState(const QString &state);

    QString getAiboxState();
    void setAiboxState(const QString &state);

protected:
    Setting(const QString &fileName, QSettings::Format format, QObject *parent = nullptr);
    Q_DISABLE_COPY(Setting)
};

#endif // SETTING_H
