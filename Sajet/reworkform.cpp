#include "reworkform.h"
#include "ui_reworkform.h"
#include "oracle_manager.h"
#include "managersajet.h"
#include <QMessageBox>
#include <qsqlquery.h>
#include <qsqlquerymodel.h>

ReworkForm::ReworkForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ReworkForm)
{
    ui->setupUi(this);
}

ReworkForm::~ReworkForm()
{
    delete ui;
}
void ReworkForm::on_pushButtonNew_clicked()
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 1. 获取当前日期前缀：RW + YYYYMMDD
    QString today = QDate::currentDate().toString("yyyyMMdd");
    QString prefix = "RW" + today;

    // 2. 查询最新记录的 REWORK_NO（按 UPDATE_TIME 降序取第一条）
    QString sql = "SELECT REWORK_NO FROM ("
                  "SELECT REWORK_NO FROM SAJET.G_REWORK_NO "
                  "WHERE REWORK_NO LIKE 'RW%' "
                  "ORDER BY UPDATE_TIME DESC"
                  ") WHERE ROWNUM = 1";

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 3. 解析尾部流水号
    int tail = 0;
    if (query.next()) {
        QString lastNo = query.value(0).toString();
        if (lastNo.length() >= 15) {
            QString tailStr = lastNo.mid(10, 5); // 索引 10~14
            bool ok;
            tail = tailStr.toInt(&ok);
            if (!ok) {
                tail = 0;
            }
        }
    }

    // 4. 生成新流水号（+1，超过99999则重置为1）
    int newTail = tail + 1;
    if (newTail > 99999) {
        newTail = 1;
    }
    QString tailStr = QString("%1").arg(newTail, 5, 10, QChar('0'));
    QString reworkNo = prefix + tailStr;

    // 5. 填入输入框
    ui->lineEditReworkno->setText(reworkNo);
}
void ReworkForm::on_lineEditInput_returnPressed()
{
    int index = ui->comboBoxInput->currentIndex();
    QString text = ui->lineEditInput->text().trimmed();

    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("不能为空"));
        return;
    }

    // 确保树有两列...
    if (ui->treeWidget->columnCount() < 2) {
        ui->treeWidget->setColumnCount(2);
        ui->treeWidget->setHeaderLabels(QStringList() << tr("条件") << tr("值"));
    }

    QString key;
    bool valid = false;

    if (index == 0) { // 序列号
        if (ManagerSajet::is_SERIAL_NUMBER(text)) {
            key = "SN";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("序列号不存在"));
        }
    } else if (index == 1) { // 箱号
        if (ManagerSajet::is_CartonNo(text)) {
            key = "CARTON";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("箱号不存在"));
        }
    } else if (index == 2) { // 重工号
        if (ManagerSajet::is_ReworkNo(text)) {
            key = "REWORK";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("重工号不存在"));
        }
    } else if (index == 3) { // 工单
        if (ManagerSajet::is_ReworkNo(text)) {
            key = "WONO";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("工单不存在"));
        }
    } else if (index == 4) { // 抽验号
        if (ManagerSajet::is_QcNo(text)) {
            key = "QCNO";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("抽验号不存在"));
        }
    } else {
        QMessageBox::warning(this, tr("未知错误"), tr("未知错误"));
    }

    if (valid) {
        // 1. 添加到树控件
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, key);
        item->setText(1, text);
        ui->treeWidget->addTopLevelItem(item);
        ui->treeWidget->expandAll();

        // 2. 同步添加到全局字典
        m_conditions.addCondition(key, text);

        // 3. 清空输入框
        ui->lineEditInput->clear();
        UpadteTable();
    }
}
void ReworkForm::UpadteTable()
{
    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = m_conditions.getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWORK");
    QStringList cartons = conditions.value("CARTON");
    QStringList worders = conditions.value("WONO");
    QStringList oqcnos = conditions.value("QCNO");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && reworks.isEmpty() && cartons.isEmpty() && worders.isEmpty() && oqcnos.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加序列号或重工号条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 3. 数据库连接检查
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 4. 构建子查询条件（转义单引号，防止SQL注入）
    QStringList subConditions;
    auto quoteAndJoin = [](const QStringList &list) -> QString {
        if (list.isEmpty()) return QString();
        QStringList quoted;
        for (const QString &item : list) {
            QString safe = item;
            safe.replace("'", "''");
            quoted << "'" + safe + "'";
        }
        return quoted.join(",");
    };

    if (!serials.isEmpty()) {
        subConditions << "S.SERIAL_NUMBER IN (" + quoteAndJoin(serials) + ")";
    }
    if (!reworks.isEmpty()) {
        subConditions << "S.REWORK_NO IN (" + quoteAndJoin(reworks) + ")";
    }
    if (!cartons.isEmpty()) {
        subConditions << "S.CARTON_NO IN (" + quoteAndJoin(cartons) + ")";
    }
    if (!worders.isEmpty()) {
        subConditions << "S.WORK_ORDER IN (" + quoteAndJoin(worders) + ")";
    }
    if (!oqcnos.isEmpty()) {
        subConditions << "S.QC_NO IN (" + quoteAndJoin(oqcnos) + ")";
    }

    QString subSql = subConditions.join(" OR ");

    if (subSql.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请添加查询条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 5. 完整查询语句（13个字段）
    QString sql = QString(
                      "SELECT S.SERIAL_NUMBER, S.WORK_ORDER, P.PART_NO, L.PDLINE_NAME, "
                      "P1.PROCESS_NAME AS WIP_PROCESS, P2.PROCESS_NAME AS CURRENT_PROCESS, "
                      "T.TERMINAL_NAME, S.CUSTOMER_SN, S.PALLET_NO, S.CARTON_NO, "
                      "S.CONTAINER, S.OUT_PDLINE_TIME, R.ROUTE_NAME "
                      "FROM SAJET.G_SN_STATUS S "
                      "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = S.MODEL_ID "
                      "LEFT JOIN SAJET.SYS_PDLINE L ON L.PDLINE_ID = S.PDLINE_ID "
                      "LEFT JOIN SAJET.SYS_PROCESS P1 ON P1.PROCESS_ID = S.WIP_PROCESS "
                      "LEFT JOIN SAJET.SYS_PROCESS P2 ON P2.PROCESS_ID = S.PROCESS_ID "
                      "LEFT JOIN SAJET.SYS_TERMINAL T ON T.TERMINAL_ID = S.TERMINAL_ID "
                      "LEFT JOIN SAJET.SYS_ROUTE R ON R.ROUTE_ID = S.ROUTE_ID "
                      "WHERE S.CURRENT_STATUS = 0 AND S.WORK_FLAG = 0 AND (%1)"
                      ).arg(subSql);

    qDebug() << "Executing SQL:" << sql;

    // 6. 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 7. 使用 QSqlQueryModel 显示结果
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }
    // ---- 处理 ROUTE_NAME ----
    QStringList routeNames;
    for (int row = 0; row < model->rowCount(); ++row) {
        QString route = model->data(model->index(row, 12)).toString(); // ROUTE_NAME 在第13列（索引12）
        if (!route.isEmpty()) {
            routeNames << route;
        }
    }

    if (routeNames.isEmpty()) {
        ui->lineEditRoute->clear();
    } else {
        QSet<QString> uniqueRoutes(routeNames.begin(), routeNames.end());
        if (uniqueRoutes.size() == 1) {
            ui->lineEditRoute->setText(*uniqueRoutes.begin());
        } else {
            QMessageBox::warning(this, tr("提示"), tr("查询结果的流程不唯一，请检查条件"));
            ui->lineEditRoute->clear();
        }
    }
    // 设置表头（13列，与SELECT顺序一致）
    QStringList headers;
    headers << tr("序列号") << tr("工单") << tr("料号") << tr("产线")
            << tr("在制工序") << tr("当前工序") << tr("终端")
            << tr("出货序号") << tr("栈板号") << tr("箱号")
            << tr("彩盒号") << tr("出线时间") << tr("途程");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 8. 设置到表格
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //修改数量显示
    ui->lcdNumber->display(model->rowCount());
}

void ReworkForm::on_lineEditRoute_returnPressed()
{
    QString routeName = ui->lineEditRoute->text().trimmed();
    if (routeName.isEmpty()) {
        ui->comboBoxProcess->clear();
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid";
        QMessageBox::warning(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT U.PROCESS_NAME "
                  "FROM SAJET.SYS_ROUTE_DETAIL D "
                  "INNER JOIN SAJET.SYS_ROUTE R ON D.ROUTE_ID = R.ROUTE_ID "
                  "INNER JOIN SAJET.SYS_PROCESS U ON D.NEXT_PROCESS_ID = U.PROCESS_ID "
                  "WHERE R.ROUTE_NAME = :route_name AND SEQ = STEP "
                  "ORDER BY D.SEQ ASC";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":route_name", routeName);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        QMessageBox::warning(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->comboBoxProcess->clear();

    bool hasData = false;
    while (query.next()) {
        QString processName = query.value(0).toString();
        ui->comboBoxProcess->addItem(processName);
        hasData = true;
    }

    if (!hasData) {
        QMessageBox::information(this, tr("提示"), tr("未找到该途程的工序步骤"));
    }
}
void ReworkForm::on_pushButtonReady_clicked()
{
    bool is_check_wo = (ui->checkBoxWo->checkState() == Qt::Checked);
    bool is_check_cusn = (ui->checkBoxCusn->checkState() == Qt::Checked);
    bool is_check_mac = (ui->checkBoxMac->checkState() == Qt::Checked);
    bool is_check_pack = (ui->checkBoxPack->checkState() == Qt::Checked);
    bool is_check_parts = (ui->checkBoxParts->checkState() == Qt::Checked);
    bool is_check_qc = (ui->checkBoxQc->checkState() == Qt::Checked);

    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = m_conditions.getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWORK");
    QStringList cartons = conditions.value("CARTON");
    QStringList worders = conditions.value("WONO");
    QStringList oqcnos = conditions.value("QCNO");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && reworks.isEmpty() && cartons.isEmpty() && worders.isEmpty() && oqcnos.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加序列号或重工号条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 3. 数据库连接检查
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 4. 构建子查询条件（转义单引号，防止SQL注入）
    QStringList subConditions;
    auto quoteAndJoin = [](const QStringList &list) -> QString {
        if (list.isEmpty()) return QString();
        QStringList quoted;
        for (const QString &item : list) {
            QString safe = item;
            safe.replace("'", "''");
            quoted << "'" + safe + "'";
        }
        return quoted.join(",");
    };

    if (!serials.isEmpty()) {
        subConditions << "S.SERIAL_NUMBER IN (" + quoteAndJoin(serials) + ")";
    }
    if (!reworks.isEmpty()) {
        subConditions << "S.REWORK_NO IN (" + quoteAndJoin(reworks) + ")";
    }
    if (!cartons.isEmpty()) {
        subConditions << "S.CARTON_NO IN (" + quoteAndJoin(cartons) + ")";
    }
    if (!worders.isEmpty()) {
        subConditions << "S.WORK_ORDER IN (" + quoteAndJoin(worders) + ")";
    }
    if (!oqcnos.isEmpty()) {
        subConditions << "S.QC_NO IN (" + quoteAndJoin(oqcnos) + ")";
    }

    QString subSql = subConditions.join(" OR ");
    if (subSql.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请添加查询条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    if(is_check_wo){
        QString new_wo = ui->lineEditWo->text().trimmed();
        int needQty = ui->lcdNumber->intValue();
        int remaining = 0;

        if (!checkWoQtyEnough(new_wo, needQty, &remaining)) {
            return; // 数量不足或出错，中断流程
        }
        if (!addInputQty(new_wo, needQty)) {
            return; // 添加工单投入数量
        }
    }
    QString route = ui->lineEditRoute->text().trimmed();
    QString process = ui->comboBoxProcess->currentText().trimmed();

    if (route.isEmpty() || process.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("途程或工序不能为空"));
        return;
    }

    // 开启事务
    if (!db.transaction()) {
        QMessageBox::critical(this, tr("错误"), tr("开启事务失败: %1").arg(db.lastError().text()));
        return;
    }

    // 构建 UPDATE 语句（使用子查询获取 SERIAL_NUMBER 列表）
    QString updateSql = QString(
                            "UPDATE SAJET.G_SN_STATUS S "
                            "SET S.ROUTE_ID = (SELECT R.ROUTE_ID FROM SAJET.SYS_ROUTE R WHERE R.ROUTE_NAME = :route), "
                            "    S.WIP_PROCESS = (SELECT P.PROCESS_ID FROM SAJET.SYS_PROCESS P WHERE P.PROCESS_NAME = :process) "
                            "WHERE S.SERIAL_NUMBER IN (%1)"
                            ).arg(subSql);
    if(is_check_cusn){

    }
    if(is_check_pack){

    }
    if(is_check_qc){

    }
    QSqlQuery updateQuery(db);
    updateQuery.prepare(updateSql);
    updateQuery.bindValue(":route", route);
    updateQuery.bindValue(":process", process);

    if (!updateQuery.exec()) {
        db.rollback();
        QMessageBox::critical(this, tr("错误"), tr("更新失败: %1").arg(updateQuery.lastError().text()));
        return;
    }

    int affectedRows = updateQuery.numRowsAffected();
    if (affectedRows == 0) {
        db.rollback();
        QMessageBox::warning(this, tr("提示"), tr("没有记录被更新，可能条件不匹配"));
        return;
    }

    // 提交事务
    if (!db.commit()) {
        QMessageBox::critical(this, tr("错误"), tr("提交事务失败: %1").arg(db.lastError().text()));
        return;
    }

    on_pushButtonClear_clicked();
    ui->lineEditInput->clear();
    ui->lineEditReworkno->clear();
    ui->lineEditRoute->clear();
    ui->lineEditWo->clear();
    ui->comboBoxProcess->clear();
}
void ReworkForm::on_lineEditWo_returnPressed()
{
    QString wo = ui->lineEditWo->text().trimmed();
    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("工单号不能为空"));
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT R.ROUTE_NAME FROM SAJET.G_WO_BASE W "
                  "LEFT JOIN SAJET.SYS_ROUTE R ON R.ROUTE_ID = W.ROUTE_ID "
                  "WHERE W.WORK_ORDER = :wo";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":wo", wo);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    if (query.next()) {
        QString routeName = query.value(0).toString();
        if (routeName.isEmpty()) {
            QMessageBox::warning(this, tr("提示"), tr("该工单未分配途程"));
            ui->lineEditRoute->clear();
            return;
        }
        ui->lineEditRoute->setText(routeName);
        on_lineEditRoute_returnPressed();
    } else {
        QMessageBox::warning(this, tr("提示"), tr("未找到该工单"));
        ui->lineEditRoute->clear();
    }
}
void ReworkForm::on_checkBoxWo_stateChanged(int arg1)
{
    bool enabled = (ui->checkBoxWo->checkState() == Qt::Checked);
    ui->lineEditWo->setEnabled(enabled);
    ui->lineEditRoute->setEnabled(!enabled);
    ui->comboBoxProcess->setEnabled(!enabled);
    if (enabled) {
        ui->lineEditWo->setFocus();
    } else{
        ui->lineEditWo->clear();
        ui->lineEditRoute->clear();
        ui->comboBoxProcess->clear();
    }
}
void ReworkForm::on_pushButtonClear_clicked()
{
    ui->treeWidget->clear();
    m_conditions.clearConditions();
}
bool ReworkForm::checkWoQtyEnough(const QString &wo, int needQty, int *remaining)
{
    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("参数错误"), tr("工单号不能为空"));
        return false;
    }
    if (needQty <= 0) {
        QMessageBox::warning(this, tr("参数错误"), tr("需求数量必须大于0"));
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return false;
    }

    QString sql = "SELECT W.TARGET_QTY, W.INPUT_QTY, W.OUTPUT_QTY FROM SAJET.G_WO_BASE W WHERE W.WORK_ORDER = :wo";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":wo", wo);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询工单数量失败: %1").arg(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        int target = query.value(0).toInt();
        int input = query.value(1).toInt();
        int output = query.value(2).toInt();
        int remain = target - output - input;

        if (remaining) {
            *remaining = remain;
        }

        if (remain < needQty) {
            QMessageBox::warning(this, tr("数量不足"),
                                 tr("工单 %1 剩余数量 %2，不足 %3，请检查")
                                     .arg(wo).arg(remain).arg(needQty));
            return false;
        }
        return true;
    } else {
        QMessageBox::warning(this, tr("提示"), tr("未找到工单 %1").arg(wo));
        return false;
    }
}
bool ReworkForm::addInputQty(const QString &wo, int qty)
{
    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("参数错误"), tr("工单号不能为空"));
        return false;
    }
    if (qty <= 0) {
        QMessageBox::warning(this, tr("参数错误"), tr("增加数量必须大于0"));
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return false;
    }

    QString sql = "UPDATE SAJET.G_WO_BASE W SET W.INPUT_QTY = W.INPUT_QTY + :qty WHERE W.WORK_ORDER = :wo";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":qty", qty);
    query.bindValue(":wo", wo);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("更新投入数量失败: %1").arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到工单 %1，更新失败").arg(wo));
        return false;
    }

    return true;
}
