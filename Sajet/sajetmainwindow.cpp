#include "sajetmainwindow.h"
#include "ui_sajetmainwindow.h"

SajetMainWindow::SajetMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SajetMainWindow)
{
    ui->setupUi(this);
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
    // 2. 叶子节点：获取存储的数据（Schema 和对象名）
    QString fullName = item->data(0, Qt::UserRole).toString();
    QString schema = item->data(0, Qt::UserRole + 1).toString();
    if (fullName.isEmpty() || schema.isEmpty()) {
        // 如果叶子节点没有数据，可能不是数据库对象（如系统功能节点），忽略
        qWarning() << "叶子节点缺少数据，可能不是数据库对象";
        return;
    }
}

