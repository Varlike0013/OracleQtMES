#ifndef SAJETMAINWINDOW_H
#define SAJETMAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class SajetMainWindow;
}

class SajetMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SajetMainWindow(QWidget *parent = nullptr);
    ~SajetMainWindow();

private:
    Ui::SajetMainWindow *ui;
};

#endif // SAJETMAINWINDOW_H
