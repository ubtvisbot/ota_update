#include "mainwindow.h"
#include <QApplication>
#include <QLockFile>
#include <QDir>
#include <QDebug>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.hide();

    QString path = QDir::temp().absoluteFilePath("ota_windows.lock");
    QLockFile lockFile(path);

    if (!lockFile.tryLock(500))
    {
        qDebug() << "process is already in running";
        return 0;
    }
    else
    {
        w.startDisplayThread();
        qDebug() << "lock file success";
    }

    return a.exec();
}
