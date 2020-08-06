#include "updatesuccesswindow.h"
#include "ui_updatesuccesswindow.h"
#include <QPalette>
#include <QBrush>
#include <QScreen>

UpdateSuccessWindow::UpdateSuccessWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UpdateSuccessWindow)
{
    ui->setupUi(this);
    setFixedSize(qApp->primaryScreen()->size());

    QPalette pl = this->palette();
    pl.setBrush(QPalette::Background, QBrush(QPixmap(":/img/resources/result/img_bg_cover.png")));
    this->setPalette(pl);

    ui->label->setStyleSheet("background: url(:/img/resources/result/img_success.png)");
    ui->label_2->setStyleSheet("font:bold; color:#71C481");
    ui->pushButton->setStyleSheet("background:#71C481; color:#FFFFFF; border:none; border-radius:5px");
    ui->pushButton->setFlat(false);

}

UpdateSuccessWindow::~UpdateSuccessWindow()
{
    delete ui;
}

void UpdateSuccessWindow::on_pushButton_clicked()
{
    close();
}
