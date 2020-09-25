#include "updatefailwindow.h"
#include "ui_updatefailwindow.h"
#include <QPalette>
#include <QBrush>
#include <QScreen>
#include <QDebug>

extern const QString kUpdate;
extern const QString kRestore;

UpdateFailWindow::UpdateFailWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UpdateFailWindow)
{
    ui->setupUi(this);
    setFixedSize(qApp->primaryScreen()->size());
//    setWindowFlags(Qt::FramelessWindowHint);
    setWindowFlags(Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip);

    QPalette pl = this->palette();
    pl.setBrush(QPalette::Background, QBrush(QPixmap(":/img/resources/result/img_bg_cover.png")));
    this->setPalette(pl);

    ui->label_img->setStyleSheet("border-image: url(:/img/resources/result/img_failed.png)");
    ui->label_fail->setStyleSheet("color:#EC7676; font:bold; font-size:20px");
    ui->label_suggest->setStyleSheet("font-size:14px; color:#4C545B; font:bold");
    ui->label_text->setStyleSheet("font-size:14px; color: #4C545B; line-height:20px;");
    QString text = "1.失败的原因可能是由于网络或者超时等因素导致\n" + QString("2.检查网络和电源正常后请回到AIBOX系统工具客户端结果页面查看并点击重试\n")
                    + "3.重试后仍然失败，请重新检查网络电源正常后，重新启动系统工具客户端操作一遍";


    ui->label_text->setText(text);
    ui->pushButton->setStyleSheet("background:#EC7676;; color:#FFFFFF; border:none; border-radius:5px");
    ui->pushButton->setFlat(false);
}

UpdateFailWindow::~UpdateFailWindow()
{
    delete ui;
}

void UpdateFailWindow::setLabelResult(QString result)
{
    if (result == kUpdate)
    {
        ui->label_fail->setText("升级失败");
    }
    else if (result == kRestore)
    {
        ui->label_fail->setText("恢复失败");
    }
}

void UpdateFailWindow::on_pushButton_clicked()
{
    qDebug() << __func__;
    hide();
}
