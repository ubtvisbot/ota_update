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
    setWindowFlags(Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip);
    setFixedSize(qApp->primaryScreen()->size());
//    setAttribute(Qt::WA_StyledBackground);
//    setStyleSheet("border-image:  url(:/img/resources/result/img_bg_cover.png)");
//    setFixedSize(300, 300);

    QPalette pl = this->palette();
    pl.setBrush(QPalette::Background, QBrush(QPixmap(":/img/resources/result/img_bg_cover.png")));
    this->setPalette(pl);

    m_pUpdateTimer = new QTimer(this);
    m_pUpdateTimer->setInterval(60);

    m_nIndex = 0;

    loadImages();

    connect(m_pUpdateTimer, &QTimer::timeout, this, &UpdateWindow::showUpdateImages);
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
        m_updatePixmapPath.append(file.absoluteFilePath());
    }
}

void UpdateWindow::showUpdateImages()
{
    int maxNum = m_updatePixmapPath.size();

    // 循环显示序列帧
    if(maxNum > 0)
    {
//        ui->label->setPixmap(*m_UpdatePixmaps.at(m_index));
        QString path = m_updatePixmapPath.at(m_nIndex);
        QString style = "border-image: url(" + path + ")";
        ui->label->setStyleSheet(style);

        m_nIndex = (m_nIndex + 1) % maxNum;
    }
}

void UpdateWindow::startShowUpdate()
{
    m_pUpdateTimer->start();
}
