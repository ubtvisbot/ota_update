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
    UpdateWindow u;
    UpdateSuccessWindow us;
    UpdateFailWindow uf;
    us.show();
    uf.show();
    u.show();
    u.startShowUpdate();
//    w.show();
    TcpServer tcpServer(&w);

    qDebug() << "home path " << QDir::homePath();

    return a.exec();
}
