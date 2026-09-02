#include "workorderinfo.h"
#include "ui_workorderinfo.h"
#include "oracle_manager.h"
#include "managersajet.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>

WorkOrderInfo::WorkOrderInfo(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WorkOrderInfo)
{
    ui->setupUi(this);
    loadPDline();
}

WorkOrderInfo::~WorkOrderInfo()
{
    delete ui;
}

void WorkOrderInfo::on_lineEditInput_returnPressed()
{
    QString input = ui->lineEditInput->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入查询内容"));
        return;
    }

    int woStatus = ui->comboBoxType->currentIndex();
    int inputType = ui->comboBoxInput->currentIndex(); // 0:工单, 1:料号, 2:流程, 3:线别

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 构建 WHERE 条件
    QStringList whereClauses;
    // 根据 inputType 决定查询字段
    switch (inputType) {
        case 0: whereClauses << "W.WORK_ORDER = :input"; break;   // 工单
        case 1: whereClauses << "P.PART_NO = :input"; break;      // 料号
        case 2: whereClauses << "R.ROUTE_NAME = :input"; break;   // 流程（路由）
        case 3: whereClauses << "PD.PDLINE_NAME = :input"; break; // 线别
        case 4: whereClauses << "W.WORK_ORDER IN (SELECT DISTINCT T.WORK_ORDER FROM SAJET.G_SN_TRAVEL T WHERE T.SERIAL_NUMBER =:input)"; break; // 线别
        default: return;
    }

    // 如果 woStatus != -1，添加状态条件 7表示全部
    if (woStatus != -1 && woStatus != 7) {
        whereClauses << "W.WO_STATUS = :status";
    }

    QString whereStr = "WHERE " + whereClauses.join(" AND ");

    // 构建完整 SQL
    QString sql = QString(
                      "SELECT W.WORK_ORDER, P.PART_NO, W.WO_RULE, W.VERSION, W.WO_STATUS, "
                      "CASE WHEN W.WO_STATUS = 0 THEN 'initial' "
                      "     WHEN W.WO_STATUS = 1 THEN 'prepare' "
                      "     WHEN W.WO_STATUS = 2 THEN 'release' "
                      "     WHEN W.WO_STATUS = 3 THEN 'work in process' "
                      "     WHEN W.WO_STATUS = 4 THEN 'hold' "
                      "     WHEN W.WO_STATUS = 5 THEN 'cancel' "
                      "     WHEN W.WO_STATUS = 6 THEN 'complete' "
                      "     ELSE 'unknown' END AS WO_STATUS_DESC, "
                      "W.TARGET_QTY, W.INPUT_QTY, W.OUTPUT_QTY, "
                      "R.ROUTE_NAME, PE.PROCESS_NAME AS START_PROCESS, "
                      "PA.PROCESS_NAME AS END_PROCESSS, PD.PDLINE_NAME "
                      "FROM SAJET.G_WO_BASE W "
                      "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = W.MODEL_ID "
                      "LEFT JOIN SAJET.SYS_ROUTE R ON R.ROUTE_ID = W.ROUTE_ID "
                      "LEFT JOIN SAJET.SYS_PROCESS PE ON PE.PROCESS_ID = W.START_PROCESS_ID "
                      "LEFT JOIN SAJET.SYS_PROCESS PA ON PA.PROCESS_ID = W.END_PROCESS_ID "
                      "LEFT JOIN SAJET.SYS_PDLINE PD ON PD.PDLINE_ID = W.DEFAULT_PDLINE_ID "
                      "%1"
                      ).arg(whereStr);

    qDebug() << "SQL:" << sql;
    qDebug() << "Input:" << input << ", Status:" << woStatus;

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":input", input);
    if (woStatus != -1) {
        query.bindValue(":status", woStatus);
    }

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }
    // 创建模型并设置查询
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }

    // 设置表头（与 SELECT 字段对应）
    QStringList headers;
    headers << tr("工单") << tr("料号") << tr("规则") << tr("版本") << tr("状态码")
            << tr("状态描述") << tr("目标数量") << tr("投入数量") << tr("产出数量")
            << tr("流程") << tr("起始工序") << tr("结束工序") << tr("产线");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}


void WorkOrderInfo::on_tableView_clicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QSqlQueryModel *model = qobject_cast<QSqlQueryModel*>(ui->tableView->model());
    if (!model) return;

    int row = index.row();
    // 假设列顺序：0-工单, 1-料号, 2-状态码, 3-状态描述, ...
    QString workOrder = model->data(model->index(row, 0)).toString();
    QString partNo    = model->data(model->index(row, 1)).toString();
    int statusCode    = model->data(model->index(row, 4)).toInt();
    QString route = model->data(model->index(row, 9)).toString();
    QString start = model->data(model->index(row, 10)).toString();
    QString end = model->data(model->index(row, 11)).toString();
    QString line = model->data(model->index(row, 12)).toString();

    // 填充到对应的 QLineEdit
    ui->lineEditWo->setText(workOrder);
    ui->lineEditPart->setText(partNo);
    ui->comboBoxStatus->setCurrentIndex(statusCode);
    ui->lineEditRoute->setText(route);

    // 查询该流程的所有工序（按顺序）
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT U.PROCESS_NAME "
                  "FROM SAJET.SYS_ROUTE_DETAIL D "
                  "INNER JOIN SAJET.SYS_ROUTE R ON D.ROUTE_ID = R.ROUTE_ID "
                  "INNER JOIN SAJET.SYS_PROCESS U ON D.NEXT_PROCESS_ID = U.PROCESS_ID "
                  "WHERE R.ROUTE_NAME = :route_name AND SEQ = STEP "
                  "ORDER BY D.SEQ ASC");
    query.bindValue(":route_name", route);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询工序失败: %1").arg(query.lastError().text()));
        return;
    }

    // 清空并填充 comboBoxStart 和 comboBoxEnd
    ui->comboBoxStart->clear();
    ui->comboBoxEnd->clear();

    int startIndex = -1;
    int endIndex = -1;
    int currentIndex = 0;

    while (query.next()) {
        QString processName = query.value(0).toString();
        ui->comboBoxStart->addItem(processName);
        ui->comboBoxEnd->addItem(processName);

        // 记录起始和结束工序的索引
        if (processName == start) {
            startIndex = currentIndex;
        }
        if (processName == end) {
            endIndex = currentIndex;
        }
        currentIndex++;
    }

    // 设置默认选中（若找到则选中，否则不选）
    if (startIndex >= 0) {
        ui->comboBoxStart->setCurrentIndex(startIndex);
    }
    if (endIndex >= 0) {
        ui->comboBoxEnd->setCurrentIndex(endIndex);
    }
    int idx = ui->comboBoxLine->findText(line);

    if (idx >= 0) {
        ui->comboBoxLine->setCurrentIndex(idx);
    } else {
        ui->comboBoxLine->setCurrentIndex(0);
    }

}
void WorkOrderInfo::loadPDline()
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT PDLINE_NAME FROM SAJET.SYS_PDLINE WHERE ENABLED = 'Y' ORDER BY PDLINE_NAME";
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("加载产线失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->comboBoxLine->clear();
    while (query.next()) {
        QString lineName = query.value(0).toString();
        ui->comboBoxLine->addItem(lineName);
    }
}

void WorkOrderInfo::on_pushButtonUpdate_clicked()
{
    QString workOrder = ui->lineEditWo->text().trimmed();
    if (workOrder.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("工单号不能为空"));
        return;
    }

    int statusCode = ui->comboBoxStatus->currentIndex();
    QString route = ui->lineEditRoute->text().trimmed();
    QString start = ui->comboBoxStart->currentText().trimmed();
    QString end   = ui->comboBoxEnd->currentText().trimmed();
    QString line  = ui->comboBoxLine->currentText().trimmed();

    if (route.isEmpty() || start.isEmpty() || end.isEmpty() || line.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请完整填写更新信息"));
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "UPDATE SAJET.G_WO_BASE W "
                  "SET W.WO_STATUS = :status, "
                  "    W.DEFAULT_PDLINE_ID = (SELECT PDLINE_ID FROM SAJET.SYS_PDLINE WHERE PDLINE_NAME = :line), "
                  "    W.ROUTE_ID = (SELECT ROUTE_ID FROM SAJET.SYS_ROUTE WHERE ROUTE_NAME = :route), "
                  "    W.START_PROCESS_ID = (SELECT PROCESS_ID FROM SAJET.SYS_PROCESS WHERE PROCESS_NAME = :startp), "
                  "    W.END_PROCESS_ID = (SELECT PROCESS_ID FROM SAJET.SYS_PROCESS WHERE PROCESS_NAME = :endp) "
                  "WHERE W.WORK_ORDER = :wo";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":status", statusCode);
    query.bindValue(":line", line);
    query.bindValue(":route", route);
    query.bindValue(":startp", start);
    query.bindValue(":endp", end);
    query.bindValue(":wo", workOrder);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("更新失败: %1").arg(query.lastError().text()));
        return;
    }

    int affected = query.numRowsAffected();
    if (affected == 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到工单 %1，更新失败").arg(workOrder));
        return;
    }
    on_lineEditInput_returnPressed(); // 重新查询当前条件
}
void WorkOrderInfo::on_lineEditRoute_returnPressed()
{
    QString route = ui->lineEditRoute->text().trimmed();
    if (!ManagerSajet::is_RouteName(route)) {
        QMessageBox::warning(this, tr("输入错误"), tr("流程名称不存在"));
        return;
    }
    ManagerSajet::loadRouteProcess(ui->comboBoxStart,route);
    ManagerSajet::loadRouteProcess(ui->comboBoxEnd,route);
}

