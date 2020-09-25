#include <QCoreApplication>
#include "tcpserver.h"
#include <QString>
#include <QDir>
#include "log.h"
#include <unistd.h>
#include "aiboxdevice.h"
#include "broadcast.h"
#include <QUuid>


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Log::instance().init();

    TcpServer tcpServer;
    Broadcast broadcast;

    broadcast.sendBroadcast();

    emResultState resultState = AiboxDevice::instance().getResultState();
    qDebug() << "getResultState " << resultState;

    if (resultState == emResultState::UpdateSuccess || resultState == emResultState::RestoreSuccess)
    {
        AiboxDevice::instance().clearUserData();
    }
    else if (resultState == emResultState::Idle)
    {
        if (AiboxDevice::instance().getAiboxState() == eAiboxUpdateFail)
        {
            resultState = UpdateImageError;
        }
    }

    tcpServer.sendOtaWindowsMsg(resultState);

    AiboxDevice::instance().resetState();

    return a.exec();
}
