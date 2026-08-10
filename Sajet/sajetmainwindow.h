#ifndef SAJETMAINWINDOW_H
#define SAJETMAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QFutureWatcher>

namespace Ui { class SajetMainWindow; }


class SajetMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SajetMainWindow(QWidget *parent = nullptr);
    ~SajetMainWindow();
private slots:
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void on_searchEdit_returnPressed();
    void on_tabWidget_tabCloseRequested(int index);

private:
    Ui::SajetMainWindow *ui;
    QMap<QString, int> m_pageTabMap;         // 页面标识 → 标签页索引
    QWidget* createWidgetFromData(const QString &pageData);
    void migrateTooltipToUserRole();
    void openPageByItemData(const QString &pageTitle, const QString &pageData);
    void setItemVisibility(QTreeWidgetItem *item, const QString &searchText, bool hasSearch);
    bool hasVisibleChild(QTreeWidgetItem *item);
    void update_user_label();
    void UpdateAsusErp();
    void ErpUpdateFinished();
    QFutureWatcher<QString> *m_erpWatcher;
};
#endif // SAJETMAINWINDOW_H
