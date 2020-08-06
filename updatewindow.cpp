#include "updatewindow.h"
#include "ui_updatewindow.h"
#include <QFileInfoList>
#include <QDir>
#include <QTimer>
#include <QScreen>
#include <QDebug>
#include <QPalette>
#include <QBrush>

UpdateWindow::UpdateWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UpdateWindow)
{
    ui->setupUi(this);
    setFixedSize(qApp->primaryScreen()->size());
//    setAttribute(Qt::WA_StyledBackground);
//    setStyleSheet("border-image:  url(:/img/resources/result/img_bg_cover.png)");

    QPalette pl = this->palette();
    pl.setBrush(QPalette::Background, QBrush(QPixmap(":/img/resources/result/img_bg_cover.png")));
    this->setPalette(pl);

    m_UpdateTimer = new QTimer(this);
    m_UpdateTimer->setInterval(60);

    m_index = 0;

    loadImages();

    connect(m_UpdateTimer, &QTimer::timeout, this, &UpdateWindow::showUpdateImages);
}

UpdateWindow::~UpdateWindow()
{
    delete ui;
}

void UpdateWindow::loadImages()
{
    QDir dirPath(":/img/resources/update");

    if(false == dirPath.isReadable())
    {
        qDebug() << "file is not readable";
        return ;
    }

    // 遍历目录
    QStringList filters;
    filters << "*.png" << "*.jpg";
    QFileInfoList fileInfoList = dirPath.entryInfoList(filters, QDir::Files, QDir::Name);
    if(fileInfoList.isEmpty())
    {
        qDebug() << "directory is empty";
        return ;
    }

    for(auto file : fileInfoList)
    {
//        std::shared_ptr<QString> (new QPixmap(file.absoluteFilePath()));
        m_UpdatePixmapPath.append(file.absoluteFilePath());
    }
}

void UpdateWindow::showUpdateImages()
{
    int maxNum = m_UpdatePixmapPath.size();

    // 循环显示序列帧
    if(maxNum > 0)
    {
//        ui->label->setPixmap(*m_UpdatePixmaps.at(m_index));
        QString path = m_UpdatePixmapPath.at(m_index);
        QString style = "border-image: url(" + path + ")";
        ui->label->setStyleSheet(style);

        m_index = (m_index + 1) % maxNum;
    }
}

void UpdateWindow::startShowUpdate()
{
    m_UpdateTimer->start();
}
