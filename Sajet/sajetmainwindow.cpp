#include <QMessageBox>
#include "sajetmainwindow.h"
#include "ui_sajetmainwindow.h"
#include "statustag.h"

SajetMainWindow::SajetMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SajetMainWindow)
{
    ui->setupUi(this);
    ui->tabWidget->clear();
    migrateTooltipToUserRole();
}

SajetMainWindow::~SajetMainWindow()
{
    delete ui;
}

void SajetMainWindow::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;
    // 1. 如果是非叶子节点（有子节点），切换展开/折叠状态
    if (item->childCount() > 0) {
        item->setExpanded(!item->isExpanded());
        return;
    }
    QString pageTitle = item->text(0);
    QString pageData = item->data(0, Qt::UserRole).toString();
    if (!pageData.isEmpty()) {
        openPageByItemData(pageTitle,pageData);
        return;
    }
}
void SajetMainWindow::migrateTooltipToUserRole() {
    QTreeWidgetItemIterator it(ui->treeWidget);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->childCount() == 0) {  // 只处理叶子节点
            QString id = item->toolTip(0);
            if (!id.isEmpty()) {
                item->setData(0, Qt::UserRole, id);   // 保存到 UserRole
                item->setToolTip(0, "");              // 清空 tooltip，关闭悬停
            }
        }
        ++it;
    }
}
void SajetMainWindow::openPageByItemData(const QString &pageTitle, const QString &pageData)
{
    if (pageData.isEmpty()) return;
    // 如果已存在该标识的 tab，切换到它
    if (m_pageTabMap.contains(pageData)) {
        int index = m_pageTabMap[pageData];
        ui->tabWidget->setCurrentIndex(index);
        return;
    }
    // 使用新函数创建 Widget
    QWidget *pageWidget = createWidgetFromData(pageData);
    if (!pageWidget) {
        // 可以在这里显示错误消息或创建默认页面
        return;
    }

    int index = ui->tabWidget->addTab(pageWidget, pageTitle);
    m_pageTabMap[pageData] = index;
    ui->tabWidget->setCurrentIndex(index);
}
QWidget* SajetMainWindow::createWidgetFromData(const QString &pageData)
{
    if (pageData == "StatusTag") {
        return new StatusTag(this);
    }  else {
        QMessageBox::critical(this, tr("错误"), tr("无法创建页面: %1").arg(pageData));
        return nullptr;
    }
}