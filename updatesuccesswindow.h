#ifndef UPDATESUCCESSWINDOW_H
#define UPDATESUCCESSWINDOW_H

#include <QWidget>

namespace Ui {
class UpdateSuccessWindow;
}

class UpdateSuccessWindow : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateSuccessWindow(QWidget *parent = 0);
    ~UpdateSuccessWindow();

private slots:
    void on_pushButton_clicked();

private:
    Ui::UpdateSuccessWindow *ui;
};

#endif // UPDATESUCCESSWINDOW_H
