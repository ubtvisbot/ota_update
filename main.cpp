#include "mainwindow.h"
#include <QApplication>
#include "tcpserver.h"
#include <QString>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    MainWindow w;
//    w.show();
    TcpServer tcpServer;

    return a.exec();
}
