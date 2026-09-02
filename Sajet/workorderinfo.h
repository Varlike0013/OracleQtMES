#ifndef WORKORDERINFO_H
#define WORKORDERINFO_H

#include <QWidget>

namespace Ui {
class WorkOrderInfo;
}

class WorkOrderInfo : public QWidget
{
    Q_OBJECT

public:
    explicit WorkOrderInfo(QWidget *parent = nullptr);
    ~WorkOrderInfo();

private slots:
    void on_lineEditInput_returnPressed();
    void on_tableView_clicked(const QModelIndex &index);
    void on_pushButtonUpdate_clicked();

    void on_lineEditRoute_returnPressed();

private:
    Ui::WorkOrderInfo *ui;
    void loadPDline();
};

#endif // WORKORDERINFO_H
