#include "displaythread.h"
#include <QDebug>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <QDir>

const static QString kKeyFilePath = "/tmp/ota_key";
const static int kKeyValue = 456701;

DisplayThread::DisplayThread(QObject *parent) : QThread(parent)
{
    m_bStop = false;
    m_msgid = -1;
}

void DisplayThread::stop()
{
    m_bStop = true;
    msgctl(m_msgid, IPC_RMID, NULL);
//    terminate();
    wait(5000);
}

void DisplayThread::run()
{
    QFile keyFile(kKeyFilePath);

    if (!keyFile.exists())
    {
        if (!keyFile.open(QIODevice::ReadWrite))
        {
            qDebug() << "open " << kKeyFilePath << " fail";
        }
        else
        {
            keyFile.close();
        }
    }

    QString cmd = "chmod 0666 " + kKeyFilePath;
    system(cmd.toStdString().data());

    key_t key = ftok(kKeyFilePath.toStdString().data(), 6);
    qDebug() << "ftok key: " << key;
    if (key < 0)
    {
        key = (key_t)kKeyValue;
    }

    m_msgid = msgget(key, IPC_CREAT | 0666);
    qDebug() << "msgget result, msgget is: " << m_msgid;

    if (m_msgid < 0)
    {
        qDebug() << "msgget error";
        return;
    }

    OtaMessage msg;
    msg.eState = Idle;
    long type = 1;

    while(!m_bStop)
    {
        int id = msgrcv(m_msgid, &msg, sizeof(msg) - sizeof(msg.type), type, 0);
        if (id >= 0)
        {
            qDebug() << "msg state: " << msg.eState << ", msg type: " << msg.type;

            if (msg.eState == Updating)
            {
                emit showUpdateWindow();
            }
            else if (msg.eState == UpdateSuccess || msg.eState == RestoreSuccess)
            {
                emit showUpdateSuccessWindow((int)msg.eState);
            }
            else if (msg.eState == UpdateImageError || msg.eState == RestoreImageError)
            {
                emit showUpdateFailWindow((int)msg.eState);
            }
        }
        else
        {
            qDebug() << "get message fail";
            sleep(1);
        }
    }
    msgctl(m_msgid, IPC_RMID, NULL);
    qDebug() << "exit";
}
