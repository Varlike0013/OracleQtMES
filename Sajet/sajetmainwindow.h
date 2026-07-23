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

private slots:
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);

private:
    Ui::SajetMainWindow *ui;
};

#endif // SAJETMAINWINDOW_H
