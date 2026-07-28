#ifndef FINDROUTE_H
#define FINDROUTE_H

#include <QWidget>
#include <QStandardItemModel>

namespace Ui {
class FindRoute;
}

class FindRoute : public QWidget
{
    Q_OBJECT

public:
    explicit FindRoute(QWidget *parent = nullptr);
    ~FindRoute();

private slots:
    void on_snlineEdit_returnPressed();

private:
    Ui::FindRoute *ui;
    QStandardItemModel *m_model = nullptr;
    void updateRouteTable(const QString &sn);
    void on_btnQuery_clicked();   // 查询按钮槽
};

#endif // FINDROUTE_H
