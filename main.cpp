#include "mainwindow.h"
#include <QApplication>
#include "tcpserver.h"
#include <QString>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    MainWindow w;
//    w.show();
    TcpServer tcpServer;

    qDebug() << "home path " << QDir::homePath();
    return a.exec();
}
