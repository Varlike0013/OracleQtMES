#ifndef CHECKROUTE_H
#define CHECKROUTE_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QListView>
#include "findroute.h"

namespace Ui {
class CheckRoute;
}
class CheckRoute : public QWidget
{
    Q_OBJECT

public:
    explicit CheckRoute(QWidget *parent = nullptr);
    ~CheckRoute();

private slots:
    void on_lineEdit_returnPressed();
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);

    void on_pushButton_clicked();

    void on_pushButtonCopy_clicked();

    void on_pushButtonFlash_clicked();

private:
    Ui::CheckRoute *ui;
    void loadProcessData();
    void loadUsefulRoutes();
    void filterTree(const QString &filter);
    QList<RouteStep> getRouteSteps(const QString &routeName);
    void displaySteps(QListView *listView, const QList<RouteStep> &steps, bool colorByNecessary);
    void fetchRouteProcess(const QStringList &mustHave, const QStringList &mustNot);
};

#endif // CHECKROUTE_H
