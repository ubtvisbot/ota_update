#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "displaythread.h"
#include "updatefailwindow.h"
#include "updatesuccesswindow.h"
#include "updatewindow.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void stopDisplayThread();
    void startDisplayThread();

private slots:
    void onShowUpdateWindow();
    void onShowUpdateFialWindow(int state);
    void onShowUpdateSuccessWindow(int state);

private:
    Ui::MainWindow *ui;

    DisplayThread m_displayThread;
    UpdateWindow m_updateWindow;
    UpdateSuccessWindow m_updateSuccessWindow;
    UpdateFailWindow m_updateFailWindow;
};

#endif // MAINWINDOW_H
