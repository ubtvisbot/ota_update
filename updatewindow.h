#ifndef UPDATEWINDOW_H
#define UPDATEWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <memory>

using namespace std;

namespace Ui {
class UpdateWindow;
}

class UpdateWindow : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateWindow(QWidget *parent = 0);
    ~UpdateWindow();
    void loadImages();
    void showUpdateImages();
    void startShowUpdate();

private:
    Ui::UpdateWindow *ui;

//    QList<std::shared_ptr<QPixmap>> m_UpdatePixmaps;
    QList<QString> m_UpdatePixmapPath;
    QTimer *m_UpdateTimer;
    int m_index;
};

#endif // UPDATEWINDOW_H
