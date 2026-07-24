#ifndef SAJETMAINWINDOW_H
#define SAJETMAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>

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
    QMap<QString, int> m_pageTabMap; // 标识 -> tab 索引
    void migrateTooltipToUserRole();
    void openPageByItemData(const QString &pageTitle, const QString &pageData);
    QWidget* createWidgetFromData(const QString &pageData);
    QString dbConnect = "Sajet";
};

#endif // SAJETMAINWINDOW_H
