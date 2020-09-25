#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

#include <QObject>
#include <QThread>
#include <QMetaType>

enum emResultState
{
    Idle = 0,               // 空闲状态
    UpdateSuccess = 1,      // 升级成功
    RestoreSuccess = 2,     // 恢复成功
    UpdateImageError = 3,	// 升级镜像错误
    RestoreImageError = 4,  // 备份镜像错误
    Updating = 5,           // 升级中
};

//Q_DECLARE_METATYPE(emResultState)

typedef struct _OtaMessage
{
    long type;
    emResultState eState;
}OtaMessage;

class DisplayThread : public QThread
{
    Q_OBJECT
public:
    DisplayThread(QObject *parent = Q_NULLPTR);

    void stop();

protected:
    void run();

signals:
    void showUpdateWindow();
    void showUpdateFailWindow(int state);
    void showUpdateSuccessWindow(int state);

private:
    bool m_bStop;

    int m_msgid;
};

#endif // DISPLAYTHREAD_H
