#ifndef UPDATEFAILWINDOW_H
#define UPDATEFAILWINDOW_H

#include <QWidget>

namespace Ui {
class UpdateFailWindow;
}

class UpdateFailWindow : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateFailWindow(QWidget *parent = 0);
    ~UpdateFailWindow();
    void setLabelResult(QString result);

private slots:
    void on_pushButton_clicked();

private:
    Ui::UpdateFailWindow *ui;
};

#endif // UPDATEFAILWINDOW_H
