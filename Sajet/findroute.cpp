#include "findroute.h"
#include "ui_findroute.h"
#include "oracle_manager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QInputDialog>

FindRoute::FindRoute(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindRoute)
{
    ui->setupUi(this);
    loadUsefulRoutes();
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

    // 收集选中的 ROUTE_NAME
    QStringList selectedNames;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QStandardItem *checkItem = m_model->item(row, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            QStandardItem *idItem = m_model->item(row, 2);  // ROUTE_ID 在第二列
            if (idItem) {
                selectedNames << idItem->text();
            }
        }
    }

    if (selectedNames.size()!=2) {
        QMessageBox::warning(this, tr("提示"), tr("必须且只能选择两个流程"));
        return;
    }

    // 将选中的两个流程名称分别赋值给 pack 和 dip，以 'P' 开头的作为 pack
    QString pack, dip;
    for (const QString &name : selectedNames) {
        if (name.startsWith('P', Qt::CaseInsensitive)) {
            pack = name;
        } else {
            dip = name;
        }
    }

    // 如果 pack 或 dip 为空，说明没有以 P 开头的名称，或两个都是 P 开头，根据业务处理
    if (pack.isEmpty() || dip.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("未找到以 P 开头的流程，请确保选择了一个 Pack 流程和一个 Dip 流程"));
        return;
    }
    UpdateTableRroute(dip,pack);
}
void FindRoute::UpdateTableRroute(QString pack, QString dip)
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 构建SQL查询（直接使用存储过程中的逻辑，但使用绑定变量）
    QString sql =
        "WITH valid_processes AS ("
        "    SELECT DISTINCT D.NEXT_PROCESS_ID"
        "    FROM SAJET.SYS_ROUTE_DETAIL D"
        "    JOIN SAJET.SYS_ROUTE R ON D.ROUTE_ID = R.ROUTE_ID"
        "    WHERE R.ROUTE_NAME IN (:dip, :pack)"
        "      AND D.NECESSARY = 'Y'"
        "      AND D.SEQ = D.STEP"
        ")"
        "SELECT R.ROUTE_NAME"
        " FROM SAJET.SYS_ROUTE R"
        " JOIN SAJET.SYS_ROUTE_DETAIL D ON R.ROUTE_ID = D.ROUTE_ID"
        " JOIN SAJET.SYS_PROCESS P ON D.PROCESS_ID = P.PROCESS_ID"
        " WHERE D.PROCESS_ID IN (SELECT NEXT_PROCESS_ID FROM valid_processes)"
        " GROUP BY R.ROUTE_NAME"
        " HAVING COUNT(DISTINCT P.PROCESS_NAME) = (SELECT COUNT(*) FROM valid_processes)"
        " ORDER BY R.ROUTE_NAME";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":dip", dip);
    query.bindValue(":pack", pack);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 使用 QStandardItemModel 填充数据
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }
    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(1);
    m_model->setHeaderData(0, Qt::Horizontal, tr("途程名称"));

    int row = 0;
    while (query.next()) {
        QString routeName = query.value(0).toString();
        QStandardItem *item = new QStandardItem(routeName);
        item->setEditable(false);
        m_model->setItem(row, 0, item);
        row++;
    }

    // 保存当前查询的 Dip 和 Pack
    m_route_info.dip = dip;
    m_route_info.pack = pack;

    ui->traveltableView->setModel(m_model);
    ui->traveltableView->resizeColumnsToContents();
    ui->traveltableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 清除选中状态
    ui->traveltableView->selectionModel()->clearSelection();
}

QList<RouteStep> FindRoute::getRouteSteps(const QString &routeName)
{
    QList<RouteStep> steps;
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid";
        return steps;
    }

    QString sql = "SELECT U.PROCESS_NAME, D.NECESSARY "
                  "FROM SAJET.SYS_ROUTE_DETAIL D "
                  "INNER JOIN SAJET.SYS_ROUTE R ON D.ROUTE_ID = R.ROUTE_ID "
                  "INNER JOIN SAJET.SYS_PROCESS U ON D.NEXT_PROCESS_ID = U.PROCESS_ID "
                  "WHERE R.ROUTE_NAME = :route_name AND SEQ = STEP "
                  "ORDER BY D.SEQ ASC";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":route_name", routeName);

    if (!query.exec()) {
        qWarning() << "Failed to get route steps:" << query.lastError().text();
        return steps;
    }

    while (query.next()) {
        RouteStep step;
        step.processName = query.value("PROCESS_NAME").toString();
        step.necessary = query.value("NECESSARY").toString().trimmed();
        steps.append(step);
    }
    return steps;
}
void FindRoute::displaySteps(QListView *listView, const QList<RouteStep> &steps, bool colorByNecessary)
{
    QStandardItemModel *model = new QStandardItemModel(listView);
    for (const RouteStep &step : steps) {
        QStandardItem *item = new QStandardItem(step.processName);
        if (colorByNecessary) {
            if (step.necessary == "Y") {
                item->setForeground(QBrush(Qt::blue));
            } else if (step.necessary == "N") {
                item->setForeground(QBrush(Qt::red));
            }
        }
        model->appendRow(item);
    }
    listView->setModel(model);
}
void FindRoute::on_pushButtonCheck_clicked()
{
    if (!m_model) {
        QMessageBox::warning(this, tr("提示"), tr("请先查询流程"));
        return;
    }

    // 获取当前选中行（表格只有一列：途程名称）
    QModelIndexList selected = ui->traveltableView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请选择一行"));
        return;
    }
    QModelIndex index = selected.first();
    QString rework = m_model->data(index).toString().trimmed();
    if (rework.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("所选行数据为空"));
        return;
    }
    m_route_info.rework = rework;
    ui->label_old->setText(tr("旧组合流程：%1 + %2").arg(m_route_info.dip, m_route_info.pack));
    ui->label_new->setText(tr("重工流程：%1").arg(rework));
    QList<RouteStep> oldSteps = getRouteSteps(m_route_info.pack);
    oldSteps.append(getRouteSteps(m_route_info.dip));
    displaySteps(ui->listView_old, oldSteps, true);
    QList<RouteStep> newSteps = getRouteSteps(rework);
    displaySteps(ui->listView_new, newSteps, true);    // 蓝色 'Y'，红色 'N'
}
void FindRoute::loadUsefulRoutes()
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT ROUTE_NAME FROM SAJET.SYS_ROUTE WHERE ENABLED = 'Y' ORDER BY ROUTE_NAME";
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qDebug() << "LoadUsefulRoutes error:" << query.lastError().text();
        QMessageBox::warning(this, tr("警告"), tr("加载途程失败"));
        return;
    }

    ui->treeWidget->clear();
    while (query.next()) {
        QString routeName = query.value(0).toString();
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, routeName);
        item->setToolTip(0, routeName);   // 悬停提示
    }
    ui->treeWidget->expandAll();          // 展开所有节点
}

void FindRoute::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;
    QString routeName = item->text(0);
    if (routeName.isEmpty()) return;

    QList<RouteStep> steps = getRouteSteps(routeName);
    displaySteps(ui->listView_new, steps, true);
}

// 输入框回车事件 – 模糊查询
void FindRoute::on_lineEditroute_returnPressed()
{
    QString filter = ui->lineEditroute->text().trimmed();
    filterTree(filter);
}

// 辅助函数：过滤树节点
void FindRoute::filterTree(const QString &filter)
{
    bool hasFilter = !filter.isEmpty();
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem(i);
        if (!item) continue;
        if (hasFilter) {
            bool match = item->text(0).contains(filter, Qt::CaseInsensitive);
            item->setHidden(!match);
        } else {
            item->setHidden(false);
        }
    }
}
void FindRoute::on_pushButtonadd_clicked()
{
    // 1. 检查必要信息是否已设置
    if (m_route_info.dip.isEmpty() || m_route_info.pack.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先查询并选择有效的 DIP 和 PACK 流程"));
        return;
    }

    // 2. 弹出输入框让用户输入新流程名称
    bool ok;
    QString router = QInputDialog::getText(this, tr("输入新流程名称"),
                                           tr("请输入新流程名称:"), QLineEdit::Normal,
                                           "", &ok);
    if (!ok || router.trimmed().isEmpty()) {
        return; // 用户取消或未输入
    }
    router = router.trimmed();

    // 3. 获取当前用户工号
    QString emp_no = OracleManager::getCurrentUsername();
    if (emp_no.isEmpty()) {
        QMessageBox::critical(this, tr("错误"), tr("无法获取当前用户信息"));
        return;
    }

    // 4. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 5. 调用存储过程
    QSqlQuery query(db);
    QString resultStr;
    resultStr.reserve(100);
    QString sql = "BEGIN SAJET.SJ_INSERT_R_ROUTE(:dip, :pack, :router, :emp_no, :result_msg); END;";
    query.prepare(sql);

    query.bindValue(":dip", m_route_info.dip);
    query.bindValue(":pack", m_route_info.pack);
    query.bindValue(":router", router);
    query.bindValue(":emp_no", emp_no);
    query.bindValue(":result_msg", resultStr, QSql::Out);

    QString result = query.boundValue(":result_msg").toString();
    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("调用存储过程失败: %1").arg(query.lastError().text()));
        return;
    }

    QString resultMsg = query.boundValue(":result_msg").toString();
    QMessageBox::information(this, tr("结果"), resultMsg.isEmpty() ? tr("操作成功") : resultMsg);

}

