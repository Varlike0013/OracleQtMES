#include "statustag.h"
#include "ui_statustag.h"
#include "oracle_manager.h"
#include <QSqlQueryModel>
#include <QSqlError>
#include <QSqlQuery>
#include <QMessageBox>

StatusTag::StatusTag(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusTag)
{
    ui->setupUi(this);
}

StatusTag::~StatusTag()
{
    delete ui;
}

void StatusTag::on_lineEdit_input_returnPressed()
{
    int index = ui->comboBox_input->currentIndex();
    QString input = ui->lineEdit_input->text().trimmed();
    if (input.isEmpty()) {
        // 可选：显示提示或清空表格
        ui->table_travel->setModel(nullptr);
        ui->table_parts->setModel(nullptr);
        return;
    }
    // 根据索引构造查询条件
    QString sql;
    switch (index) {
    case 0: //序号查询
        sql = QString("SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE S.SERIAL_NUMBER = '%1'").arg(input);
        break;
    case 1: //料件
        sql = QString("SELECT K.SERIAL_NUMBER FROM SAJET.G_SN_KEYPARTS K WHERE K.ITEM_PART_SN = '%1'").arg(input);
        break;
    case 2: //mac
        sql = QString("SELECT M.SERIAL_NUMBER FROM SAJET.G_WO_MAC M WHERE M.MAC = '%1'").arg(input);
        break;
    case 3: //SSN
        sql = QString("SELECT M.SERIAL_NUMBER FROM SAJET.G_WO_MAC M WHERE M.CUSTOMER_SN = '%1'").arg(input);
        break;
    case 4: //出货序号
        sql = QString("SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE S.CUSTOMER_SN = '%1'").arg(input);
        break;
    default:
        return;
    }
    // 获取数据库连接
    QSqlDatabase m_db = OracleManager::instance().getCurrentDbMain();
    if (!m_db.isValid() || !m_db.isOpen()) {
        qDebug() << "Database connection is invalid or not open.";
        ui->table_travel->setModel(nullptr);
        ui->table_parts->setModel(nullptr);
        return;
    }

    // 执行查询
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qDebug() << "Query error:" << query.lastError().text();
        ui->table_travel->setModel(nullptr);
        ui->table_parts->setModel(nullptr);
        return;
    }

    // 收集所有序列号
    QStringList serialNumbers;
    while (query.next()) {
        serialNumbers << query.value(0).toString();
    }

    // 判断结果数量
    if (serialNumbers.isEmpty()) {
        // 无匹配 → 清空表格
        ui->table_travel->setModel(nullptr);
        ui->table_parts->setModel(nullptr);
        return;
    }

    if (serialNumbers.size() > 1) {
        // 多个匹配 → 弹窗提示
        QMessageBox::information(this, tr("提示"),
                                 tr("查询到 %1 个匹配的序列号，将显示第一个：%2")
                                     .arg(serialNumbers.size())
                                     .arg(serialNumbers.first()));
    }

    // 使用第一个序列号更新两个表格
    QString serial_number = serialNumbers.first();
    // 更新travel表和parts表
    update_table_travel(serial_number);
    update_table_parts(serial_number);
}
void StatusTag::update_table_travel(QString serial_number)
{
    // 1. 如果序列号为空，清空表格
    if (serial_number.isEmpty()) {
        ui->table_travel->setModel(nullptr);
        return;
    }

    // 2. 获取主数据库连接
    QSqlDatabase m_db = OracleManager::instance().getCurrentDbMain();
    if (!m_db.isValid() || !m_db.isOpen()) {
        qDebug() << "Database connection is invalid or not open.";
        ui->table_travel->setModel(nullptr);
        return;
    }

    // 3. 安全转义输入（防止 SQL 注入）
    QString safeSerial = serial_number;
    safeSerial.replace("'", "''");

    // 4. 构建复杂查询（使用注释中的多表连接）
    QString travelSql = QString(
                            "SELECT T.WORK_ORDER, P.PART_NO, PL.PDLINE_NAME, PR.PROCESS_NAME, T.WORK_FLAG, "
                            "T.OUT_PROCESS_TIME, TE.TERMINAL_NAME, E.EMP_NAME, C.CUSTOMER_NAME, T.CUSTOMER_SN, "
                            "T.QC_NO, T.REWORK_NO, T.PANEL_NO "
                            "FROM SAJET.G_SN_TRAVEL T "
                            "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = T.MODEL_ID "
                            "LEFT JOIN SAJET.SYS_PDLINE PL ON PL.PDLINE_ID = T.PDLINE_ID "
                            "LEFT JOIN SAJET.SYS_PROCESS PR ON PR.PROCESS_ID = T.PROCESS_ID "
                            "LEFT JOIN SAJET.SYS_TERMINAL TE ON TE.TERMINAL_ID = T.TERMINAL_ID "
                            "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = T.EMP_ID "
                            "LEFT JOIN SAJET.SYS_CUSTOMER C ON C.CUSTOMER_ID = T.CUSTOMER_ID "
                            "WHERE T.SERIAL_NUMBER = '%1' "
                            "ORDER BY T.OUT_PROCESS_TIME"
                            ).arg(safeSerial);

    // 5. 删除旧模型（避免内存泄漏）
    QAbstractItemModel *oldModel = ui->table_travel->model();
    if (oldModel) {
        ui->table_travel->setModel(nullptr);
        oldModel->deleteLater();
    }

    // 6. 创建新模型并执行查询
    QSqlQueryModel *travelModel = new QSqlQueryModel(this);
    travelModel->setQuery(travelSql, m_db);

    if (travelModel->lastError().isValid()) {
        qDebug() << "Travel query error:" << travelModel->lastError().text();
        travelModel->deleteLater();   // 错误时释放模型
        return;
    }

    // 7. 设置模型并自适应列宽
    ui->table_travel->setModel(travelModel);
    ui->table_travel->resizeColumnsToContents();
}
void StatusTag::update_table_parts(QString serial_number)
{
    // 1. 如果序列号为空，清空表格
    if (serial_number.isEmpty()) {
        ui->table_parts->setModel(nullptr);
        return;
    }

    // 2. 获取主数据库连接
    QSqlDatabase m_db = OracleManager::instance().getCurrentDbMain();
    if (!m_db.isValid() || !m_db.isOpen()) {
        qDebug() << "Database connection is invalid or not open.";
        ui->table_parts->setModel(nullptr);
        return;
    }

    // 3. 安全转义输入（防止 SQL 注入）
    QString safeSerial = serial_number;
    safeSerial.replace("'", "''");

    // 4. 构建复杂查询（多表连接）
    QString partsSql = QString(
                           "SELECT P.PART_NO, K.VERSION, P.SPEC1, K.ITEM_PART_SN, P.PART_TYPE, E.EMP_NAME "
                           "FROM SAJET.G_SN_KEYPARTS K "
                           "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = K.ITEM_PART_ID "
                           "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = K.UPDATE_USERID "
                           "WHERE K.SERIAL_NUMBER = '%1' "
                           "ORDER BY K.UPDATE_TIME"
                           ).arg(safeSerial);

    // 5. 删除旧模型（避免内存泄漏）
    QAbstractItemModel *oldModel = ui->table_parts->model();
    if (oldModel) {
        ui->table_parts->setModel(nullptr);
        oldModel->deleteLater();
    }

    // 6. 创建新模型并执行查询
    QSqlQueryModel *partsModel = new QSqlQueryModel(this);
    partsModel->setQuery(partsSql, m_db);

    if (partsModel->lastError().isValid()) {
        qDebug() << "Parts query error:" << partsModel->lastError().text();
        partsModel->deleteLater();
        return;
    }

    // 7. 设置模型并自适应列宽
    ui->table_parts->setModel(partsModel);
    ui->table_parts->resizeColumnsToContents();
}
