#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

extern const QString kUpdate = "update";
extern const QString kRestore = "restore";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_updateFailWindow.hide();
    m_updateSuccessWindow.hide();
    m_updateWindow.hide();

    qDebug() << "wait to display window...";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::stopDisplayThread()
{
    qDebug() << __func__;
    m_displayThread.stop();
}

void MainWindow::startDisplayThread()
{
    qDebug() << __func__;
    m_displayThread.start();

    connect(&m_displayThread, &DisplayThread::showUpdateFailWindow, this, &MainWindow::onShowUpdateFialWindow);
    connect(&m_displayThread, &DisplayThread::showUpdateSuccessWindow, this, &MainWindow::onShowUpdateSuccessWindow);
    connect(&m_displayThread, &DisplayThread::showUpdateWindow, this, &MainWindow::onShowUpdateWindow);
}

void MainWindow::onShowUpdateWindow()
{
    qDebug() << __func__;
    m_updateWindow.show();
    m_updateWindow.startShowUpdate();

    m_updateSuccessWindow.hide();
    m_updateFailWindow.hide();

}

void MainWindow::onShowUpdateFialWindow(int state)
{
    qDebug() << __func__;
    qDebug() << "state is " << state;

    if (state == UpdateImageError)
    {
        m_updateFailWindow.setLabelResult(kUpdate);
    }
    else if (state == RestoreImageError)
    {
        m_updateFailWindow.setLabelResult(kRestore);
    }

    m_updateFailWindow.showFullScreen();
    m_updateSuccessWindow.hide();
    m_updateWindow.hide();
}

void MainWindow::onShowUpdateSuccessWindow(int state)
{
    qDebug() << __func__;
    qDebug() << "state is " << state;

    if (state == UpdateSuccess)
    {
        m_updateSuccessWindow.setLabelResult(kUpdate);
    }
    else if (state == RestoreSuccess)
    {
        m_updateSuccessWindow.setLabelResult(kRestore);
    }

    m_updateSuccessWindow.showFullScreen();
    m_updateFailWindow.hide();
    m_updateWindow.hide();
}
