#ifndef GEDTMAINWINDOW_H
#define GEDTMAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class GedtMainWindow;
}

class GedtMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GedtMainWindow(QWidget *parent = nullptr);
    ~GedtMainWindow();

private:
    Ui::GedtMainWindow *ui;
};

#endif // GEDTMAINWINDOW_H
