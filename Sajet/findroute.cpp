#include "findroute.h"
#include "ui_findroute.h"
#include "oracle_manager.h"
#include <QMessageBox>
#include <qsqlquery.h>

FindRoute::FindRoute(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindRoute)
{
    ui->setupUi(this);
}

FindRoute::~FindRoute()
{
    delete ui;
}


void FindRoute::on_snlineEdit_returnPressed()
{
    QString sn = ui->snlineEdit->text().trimmed();
    if (sn.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入序列号"));
        return;
    }
    updateRouteTable(sn);
}
void FindRoute::updateRouteTable(const QString &sn)
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 准备查询
    QString sql = "SELECT ROUTE_ID, ROUTE_NAME FROM SAJET.SYS_ROUTE "
                  "WHERE ROUTE_ID IN (SELECT ROUTE_ID FROM SAJET.G_SN_TRAVEL "
                  "WHERE SERIAL_NUMBER = :sn GROUP BY ROUTE_ID)";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sn", sn);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("查询失败"), tr("数据库查询出错: %1").arg(query.lastError().text()));
        return;
    }

    // 创建标准模型
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }
    m_model = new QStandardItemModel(this);
    int colCount = 3;  // 复选框 + ROUTE_ID + ROUTE_NAME
    m_model->setColumnCount(colCount);

    // 设置表头（可翻译）
    QStringList headers;
    headers << tr("选择") << tr("路线ID") << tr("路线名称");
    m_model->setHorizontalHeaderLabels(headers);

    while (query.next()) {
        QList<QStandardItem*> rowItems;

        // 复选框列（第一列）
        QStandardItem *checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setEditable(false);
        checkItem->setTextAlignment(Qt::AlignCenter);
        rowItems << checkItem;

        // 数据列
        QString routeId = query.value("ROUTE_ID").toString();
        QString routeName = query.value("ROUTE_NAME").toString();

        QStandardItem *idItem = new QStandardItem(routeId);
        idItem->setEditable(false);
        rowItems << idItem;

        QStandardItem *nameItem = new QStandardItem(routeName);
        nameItem->setEditable(false);
        rowItems << nameItem;

        m_model->appendRow(rowItems);
    }

    // 显示到表格
    ui->traveltableView->setModel(m_model);
    ui->traveltableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->traveltableView->resizeColumnsToContents();
    ui->traveltableView->verticalHeader()->setVisible(false);
}
void FindRoute::on_pushButtonselect_clicked()
{
    if (!m_model) {
        QMessageBox::warning(this, tr("提示"), tr("请先查询流程"));
        return;
    }

    // 收集选中的 ROUTE_ID
    QStringList selectedIds;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QStandardItem *checkItem = m_model->item(row, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            QStandardItem *idItem = m_model->item(row, 1);  // ROUTE_ID 在第二列
            if (idItem) {
                selectedIds << idItem->text();
            }
        }
    }

    if (selectedIds.size()!=2) {
        QMessageBox::warning(this, tr("提示"), tr("必须且只能选择两个流程"));
        return;
    }

    // 示例：根据选中的 ROUTE_ID 进行下一步查询（例如查询路线步骤）
    QString idList = selectedIds.join("','");
    qDebug()<<idList;
}

