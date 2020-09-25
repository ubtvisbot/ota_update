#include "updatesuccesswindow.h"
#include "ui_updatesuccesswindow.h"
#include <QPalette>
#include <QBrush>
#include <QScreen>
#include <QDebug>

extern const QString kUpdate;
extern const QString kRestore;

UpdateSuccessWindow::UpdateSuccessWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UpdateSuccessWindow)
{
    ui->setupUi(this);
    setFixedSize(qApp->primaryScreen()->size());
//    setWindowFlags(Qt::FramelessWindowHint);
    setWindowFlags(Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip);

    QPalette pl = this->palette();
    pl.setBrush(QPalette::Background, QBrush(QPixmap(":/img/resources/result/img_bg_cover.png")));
    this->setPalette(pl);

    ui->label->setStyleSheet("border-image: url(:/img/resources/result/img_success.png)");
    ui->label_success->setStyleSheet("font:bold; color:#71C481");
    ui->pushButton->setStyleSheet("background:#71C481; color:#FFFFFF; border:none; border-radius:5px");
    ui->pushButton->setFlat(false);

}

UpdateSuccessWindow::~UpdateSuccessWindow()
{
    delete ui;
}

void UpdateSuccessWindow::setLabelResult(QString result)
{
    if (result == kUpdate)
    {
        ui->label_success->setText("升级成功");
    }
    else if (result == kRestore)
    {
        ui->label_success->setText("恢复成功");
    }
}

void UpdateSuccessWindow::on_pushButton_clicked()
{
    qDebug() << __func__;
    hide();
}
