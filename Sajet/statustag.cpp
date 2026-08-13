#include "statustag.h"
#include "ui_statustag.h"
#include "oracle_manager.h"
#include <QSqlQueryModel>
#include <QSqlError>
#include <QSqlQuery>
#include <QMessageBox>
#include <QStandardItem>

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
    update_status_label(serial_number);
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
                            "SELECT T.WORK_ORDER, P.PART_NO, PL.PDLINE_NAME, PR.PROCESS_NAME, T.CURRENT_STATUS, "
                            "TO_CHAR(T.OUT_PROCESS_TIME, 'YYYY/MM/DD HH24:MI:SS') AS OUT_PROCESS_TIME,"
                            "TE.TERMINAL_NAME, E.EMP_NAME, C.CUSTOMER_NAME, T.CUSTOMER_SN, "
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

    // 5. 执行查询
    QSqlQuery query(m_db);
    if (!query.exec(travelSql)) {
        qDebug() << "Travel query error:" << query.lastError().text();
        ui->table_travel->setModel(nullptr);
        return;
    }

    // 6. 创建标准模型
    const int colCount = 13; // 字段数
    QStandardItemModel *standardModel = new QStandardItemModel(this);
    standardModel->setColumnCount(colCount);

    // 7. 可翻译表头
    QStringList headers;
    headers << tr("工单") << tr("料号") << tr("生产线") << tr("工序") << tr("状态")
            << tr("完成时间") << tr("终端") << tr("作业人员") << tr("客户")
            << tr("出货序号") << tr("质检编号") << tr("重工编号") << tr("电压值");
    for (int i = 0; i < headers.size(); ++i) {
        standardModel->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 8. 填充数据
    int rowIndex = 0;
    while (query.next()) {
        QList<QStandardItem*> rowItems;
        int current_status = 0;

        for (int c = 0; c < colCount; ++c) {
            QVariant value = query.value(c);
            QString displayText;

            if (c == 4) { // WORK_FLAG 列
                current_status = value.toInt();
                switch (current_status) {
                case 0: displayText = tr("OK"); break;
                case 1: displayText = tr("NG"); break;
                default: displayText = QString::number(current_status); break;
                }
            } else {
                displayText = value.toString();
            }

            QStandardItem *item = new QStandardItem(displayText);
            item->setTextAlignment(Qt::AlignCenter);
            rowItems.append(item);
        }

        // 确定背景色（优先级：WORK_FLAG=1 > 偶数行 > 白色）
        QColor bgColor;
        if (current_status == 1) {
            bgColor = QColor(200, 50, 50);      // 柔和红色
        } else if (rowIndex % 2 == 0) {
            bgColor = QColor(200, 220, 240);    // 柔和灰蓝
        } else {
            bgColor = QColor(240, 240, 240);    // 浅灰白
        }

        // 为整行所有单元格设置背景色
        for (QStandardItem *item : rowItems) {
            item->setBackground(bgColor);
        }

        standardModel->appendRow(rowItems);
        rowIndex++;
    }

    // 9. 替换旧模型
    QAbstractItemModel *oldModel = ui->table_travel->model();
    if (oldModel) {
        ui->table_travel->setModel(nullptr);
        oldModel->deleteLater();
    }

    ui->table_travel->setModel(standardModel);
    ui->table_travel->resizeColumnsToContents();
    ui->table_travel->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
                           "SELECT P.PART_NO, K.VERSION, P.SPEC1, K.ITEM_PART_SN, P.PART_TYPE, E.EMP_NAME, "
                           "TO_CHAR(K.UPDATE_TIME, 'YYYY/MM/DD HH24:MI:SS') AS UPDATE_TIME "
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

    // 自定义可翻译的表头（列数与查询字段数一致）
    QStringList headers;
    headers << tr("零件号")<< tr("版本")<< tr("规格")
            << tr("关键件序列号") << tr("零件类型") << tr("操作员") << tr("执行时间");   // E.EMP_NAME
    for (int i = 0; i < headers.size(); ++i) {
        partsModel->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 7. 设置模型并自适应列宽
    ui->table_parts->setModel(partsModel);
    ui->table_parts->resizeColumnsToContents();
}
void StatusTag::update_status_label(QString serial_number)
{
    // 1. 如果序列号为空，清空所有标签
    if (serial_number.isEmpty()) {
        clearLabels("");
        return;
    }
    // 2. 获取主数据库连接
    QSqlDatabase m_db = OracleManager::instance().getCurrentDbMain();
    if (!m_db.isValid() || !m_db.isOpen()) {
        qDebug() << "Database connection is invalid or not open.";
        clearLabels("");
        return;
    }
    // 3. 安全转义输入（防止 SQL 注入）
    QString safeSerial = serial_number;
    safeSerial.replace("'", "''");
    // 4. 构建 SQL 查询（多表连接）
    QString sql = QString(
                      "SELECT S.WORK_ORDER, P.PART_NO, P.SPEC1, "
                      "PR.PROCESS_NAME, S.WORK_FLAG, S.CUSTOMER_SN, "
                      "S.QC_NO, S.REWORK_NO, S.CARTON_NO, "
                      "R.ROUTE_NAME, M.MAC, M.CUSTOMER_SN AS SSN, PP.PCB_QRCODE "
                      "FROM SAJET.G_SN_STATUS S "
                      "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = S.MODEL_ID "
                      "LEFT JOIN SAJET.SYS_PROCESS PR ON PR.PROCESS_ID = S.WIP_PROCESS "
                      "LEFT JOIN SAJET.SYS_ROUTE R ON R.ROUTE_ID = S.ROUTE_ID "
                      "LEFT JOIN SAJET.G_WO_MAC M ON M.SERIAL_NUMBER = S.SERIAL_NUMBER "
                      "LEFT JOIN SAJET.ECS_PPID_PCB_CODE PP ON PP.STRSMTSN = S.SERIAL_NUMBER "
                      "WHERE S.SERIAL_NUMBER = '%1' AND ROWNUM = 1"
                      ).arg(safeSerial);

    // 5. 执行查询
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qDebug() << "Query error:" << query.lastError().text();
        clearLabels("");
        return;
    }
    // 6. 处理结果
    if (query.next()) {
        // 按 SELECT 顺序索引：0=WORK_ORDER, 1=PART_NO, 2=SPEC1, 3=PROCESS_NAME,
        // 4=WORK_FLAG, 5=CUSTOMER_SN, 6=QC_NO, 7=REWORK_NO, 8=CARTON_NO,
        // 9=ROUTE_NAME, 10=MAC, 11=SSN, 12=PCB_QRCODE
        ui->Label_SN->setText(serial_number);
        ui->label_WO->setText(query.value(0).toString());
        ui->label_partno->setText(query.value(1).toString());
        ui->label_partdesc->setText(query.value(2).toString());
        ui->label_processnext->setText(query.value(3).toString());
        int work_flag = 0;
        work_flag =  query.value(4).toInt();
        QString displayText = "Good";
        switch (work_flag) {
            case 0: displayText = tr("Good"); break;
            case 1: displayText = tr("Repair"); break;
            case 2: displayText = tr("Hold"); break;
            default: displayText = QString::number(work_flag); break;
        }
        ui->label_wokeflag->setText(displayText);
        ui->label_csn->setText(query.value(5).toString());
        // QC_NO 和 REWORK_NO 未直接显示，但可以保留
        ui->label_carton->setText(query.value(8).toString());
        ui->label_Route->setText(query.value(9).toString());
        ui->label_mac->setText(query.value(10).toString());
        ui->label_ssn->setText(query.value(11).toString());
        ui->label_ppid->setText(query.value(12).toString());
    } else {
        // 无匹配记录：保留序列号，其他标签清空
        clearLabels(serial_number);
    }
}
void StatusTag::clearLabels(QString serial_number)
{
    ui->Label_SN->setText(serial_number);
    ui->label_WO->setText("");
    ui->label_partno->setText("");
    ui->label_partdesc->setText("");
    ui->label_processnext->setText("");
    ui->label_wokeflag->setText("");
    ui->label_csn->setText("");
    ui->label_carton->setText("");
    ui->label_Route->setText("");
    ui->label_mac->setText("");
    ui->label_ssn->setText("");
    ui->label_ppid->setText("");
}