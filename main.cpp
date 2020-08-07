#include "mainwindow.h"
#include <QApplication>
#include "tcpserver.h"
#include <QString>
#include <QDir>
#include "log.h"
#include "updatewindow.h"
#include "updatesuccesswindow.h"
#include "updatefailwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Log::instance().init();
    MainWindow w;
    w.hide();
    UpdateWindow uw;
    UpdateSuccessWindow usw;
    UpdateFailWindow ufw;
//    usw.show();
//    ufw.show();
//    uw.show();
//    uw.startShowUpdate();
//    w.show();
    TcpServer tcpServer(&uw);

    QString resultState = tcpServer.getResultState();

    if (resultState.toInt() == emResultState::RestoreSuccess || resultState.toInt() == emResultState::UpdateSuccess)
    {
//        UpdateSuccessWindow usw;
        usw.show();
    }
    else if ( resultState == "" || resultState.toInt() == emResultState::Idle)
    {
        // do nothing
        // 空闲状态，不用处理
    }
    else
    {
//        UpdateFailWindow ufw;
        ufw.show();
    }

    emAiboxState state = tcpServer.switchResultStateToAiboxState(resultState.toInt());
    qDebug() << "state " << state;

    qDebug() << "home path " << QDir::homePath();

    return a.exec();
}
