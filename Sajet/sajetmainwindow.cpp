#include <QMessageBox>
#include "sajetmainwindow.h"
#include "ui_sajetmainwindow.h"
#include "statustag.h"
#include "reworkquestion.h"


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
    QApplication::quit();
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
        return new StatusTag(this); //返回对应的类
    }else if (pageData == "ReworkQuestion") {
        return new ReworkQuestion(this); //返回对应的类
    }  else {
        QMessageBox::critical(this, tr("错误"), tr("无法创建页面: %1").arg(pageData));
        return nullptr;
    }
}
void SajetMainWindow::on_searchEdit_returnPressed()
{
    QString text = ui->searchEdit->text();
    QString searchText = text.trimmed();
    bool hasSearch = !searchText.isEmpty();

    // 遍历顶层分类节点（"表"、"存储过程"）
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = ui->treeWidget->topLevelItem(i);
        if (!category) continue;

        // 递归设置可见性
        setItemVisibility(category, searchText, hasSearch);

        // 展开/折叠分类节点
        if (hasSearch) {    // 如果有可见子节点则展开，否则折叠
            category->setExpanded(hasVisibleChild(category));
        } else {    // 无搜索时全部折叠
            category->setExpanded(false);
        }
    }
}
void SajetMainWindow::setItemVisibility(QTreeWidgetItem *item, const QString &searchText, bool hasSearch)
{
    if (!item) return;

    bool isLeaf = (item->childCount() == 0);

    if (isLeaf) {
        // 叶子节点：匹配全名（不区分大小写）
        if (hasSearch) {
            QString name = item->text(0);
            bool match = name.contains(searchText, Qt::CaseInsensitive);
            item->setHidden(!match);
        } else {
            item->setHidden(false); // 无搜索时全部显示
        }
    } else {
        // 非叶子节点：先递归处理所有子节点
        for (int i = 0; i < item->childCount(); ++i) {
            setItemVisibility(item->child(i), searchText, hasSearch);
        }
        // 根据是否有可见子节点决定自身是否隐藏
        bool hasVisible = hasVisibleChild(item);
        item->setHidden(!hasVisible);

        // 如果处于搜索模式且自身可见，则展开（显示路径）
        if (hasSearch) {
            item->setExpanded(hasVisible);
        } else {
            item->setExpanded(true);
        }
    }
}
bool SajetMainWindow::hasVisibleChild(QTreeWidgetItem *item)
{
    if (!item) return false;
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (!child->isHidden()) {
            return true;
        }
        if (hasVisibleChild(child)) {
            return true;
        }
    }
    return false;
}