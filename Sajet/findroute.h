#ifndef FINDROUTE_H
#define FINDROUTE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QListWidget>
#include <QTreeWidgetItem>

namespace Ui {
class FindRoute;
}
struct RouteInfo {
    QString dip;
    QString pack;
    QString rework;
};
struct RouteStep {
    QString processName;
    QString necessary;   // "Y" 或 "N"
};
class FindRoute : public QWidget
{
    Q_OBJECT

public:
    explicit FindRoute(QWidget *parent = nullptr);
    ~FindRoute();

private slots:
    void on_snlineEdit_returnPressed();
    void on_pushButtonselect_clicked();
    void on_pushButtonCheck_clicked();
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void on_lineEditroute_returnPressed();
    void on_pushButtonadd_clicked();

private:
    Ui::FindRoute *ui;
    QStandardItemModel *m_model = nullptr;
    RouteInfo m_route_info;
    void updateRouteTable(const QString &sn);
    void UpdateTableRroute(QString pack,QString dip);
    QList<RouteStep> getRouteSteps(const QString &routeName);
    void displaySteps(QListView *listView, const QList<RouteStep> &steps, bool colorByNecessary);
    void loadUsefulRoutes();
    void filterTree(const QString &filter);
};

#endif // FINDROUTE_H
