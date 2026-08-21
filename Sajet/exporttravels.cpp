#include "exporttravels.h"
#include "ui_exporttravels.h"
#include "oracle_manager.h"
#include "managersajet.h"
#include <QMessageBox>
#include <QSqlQueryModel>
#include <QFile>
#include <QFileDialog>

ExportTravels::ExportTravels(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ExportTravels)
{
    ui->setupUi(this);
    ManagerSajet::loadPDline(ui->comboBoxLine);
    ui->comboBoxLine->setCurrentIndex(-1);
    ui->comboBoxProcess->setCurrentIndex(-1);
    ui->comboBoxTerminal->setCurrentIndex(-1);

    QDateTime startDateTime = QDateTime::currentDateTime().addDays(-3);
    ui->dateTimeEditStart->setDateTime(startDateTime);
    ui->dateTimeEditEnd->setDateTime(QDateTime::currentDateTime());
}

ExportTravels::~ExportTravels()
{
    delete ui;
}

void ExportTravels::on_pushButtonSelect_clicked()
{
    // 1. 获取输入并去除首尾空格
    QString wo = ui->lineEditWo->text().trimmed();
    QString part = ui->lineEditPart->text().trimmed();
    QString line = ui->comboBoxLine->currentText().trimmed();
    QString process = ui->comboBoxProcess->currentText().trimmed();
    QString terminal = ui->comboBoxTerminal->currentText().trimmed();
    QDateTime timeStart = ui->dateTimeEditStart->dateTime();
    QDateTime timeEnd = ui->dateTimeEditEnd->dateTime();

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 3. 动态构建 WHERE 条件
    QStringList whereClauses;
    QMap<QString, QVariant> bindValues; // 存储绑定的参数名和值

    // 工单
    if (!wo.isEmpty()) {
        whereClauses << "T.WORK_ORDER = :wo";
        bindValues[":wo"] = wo;
    }
    // 料号
    if (!part.isEmpty()) {
        whereClauses << "T.MODEL_ID = (SELECT PART_ID FROM SAJET.SYS_PART WHERE PART_NO = :part)";
        bindValues[":part"] = part;
    }
    // 产线
    if (!line.isEmpty()) {
        whereClauses << "T.PDLINE_ID = (SELECT PDLINE_ID FROM SAJET.SYS_PDLINE WHERE PDLINE_NAME = :line)";
        bindValues[":line"] = line;
    }
    // 工序
    if (!process.isEmpty()) {
        whereClauses << "T.PROCESS_ID = (SELECT PROCESS_ID FROM SAJET.SYS_PROCESS WHERE PROCESS_NAME = :process)";
        bindValues[":process"] = process;
    }
    // 终端
    if (!terminal.isEmpty()) {
        whereClauses << "T.TERMINAL_ID = (SELECT TERMINAL_ID FROM SAJET.SYS_TERMINAL WHERE TERMINAL_NAME = :terminal)";
        bindValues[":terminal"] = terminal;
    }
    // 时间范围（如果两个时间都有效才添加，如果只有一个则根据情况？这里按常见做法：若开始和结束都有效才加范围）
    if (timeStart.isValid() && timeEnd.isValid()) {
        // 注意：BETWEEN 包含两端，若需排除结束时间可使用 < 和 >
        whereClauses << "T.OUT_PROCESS_TIME BETWEEN :startTime AND :endTime";
        bindValues[":startTime"] = timeStart;
        bindValues[":endTime"] = timeEnd;
    } else if (timeStart.isValid()) {
        whereClauses << "T.OUT_PROCESS_TIME >= :startTime";
        bindValues[":startTime"] = timeStart;
    } else if (timeEnd.isValid()) {
        whereClauses << "T.OUT_PROCESS_TIME <= :endTime";
        bindValues[":endTime"] = timeEnd;
    }

    // 4. 构建完整 SQL
    QString sql = "SELECT T.WORK_ORDER, P.PART_NO, T.SERIAL_NUMBER, PL.PDLINE_NAME, PR.PROCESS_NAME, T.CURRENT_STATUS, "
                  "TO_CHAR(T.OUT_PROCESS_TIME, 'YYYY/MM/DD HH24:MI:SS') AS OUT_PROCESS_TIME, "
                  "TE.TERMINAL_NAME, E.EMP_NAME, C.CUSTOMER_NAME, T.CUSTOMER_SN, T.QC_NO, T.REWORK_NO, T.PANEL_NO "
                  "FROM SAJET.G_SN_TRAVEL T "
                  "LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = T.MODEL_ID "
                  "LEFT JOIN SAJET.SYS_PDLINE PL ON PL.PDLINE_ID = T.PDLINE_ID "
                  "LEFT JOIN SAJET.SYS_PROCESS PR ON PR.PROCESS_ID = T.PROCESS_ID "
                  "LEFT JOIN SAJET.SYS_TERMINAL TE ON TE.TERMINAL_ID = T.TERMINAL_ID "
                  "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = T.EMP_ID "
                  "LEFT JOIN SAJET.SYS_CUSTOMER C ON C.CUSTOMER_ID = T.CUSTOMER_ID ";

    if (!whereClauses.isEmpty()) {
        sql += "WHERE " + whereClauses.join(" AND ") + " ";
    }
    sql += "ORDER BY T.OUT_PROCESS_TIME DESC";

    // 5. 准备并执行查询
    QSqlQuery query(db);
    query.prepare(sql);
    for (auto it = bindValues.begin(); it != bindValues.end(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 6. 显示结果（使用 QSqlQueryModel）
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }

    // 7. 设置自定义表头
    QStringList headers;
    headers << tr("工单") << tr("料号") << tr("序列号") << tr("产线") << tr("工序")
            << tr("当前状态") << tr("产出时间") << tr("终端") << tr("员工")
            << tr("客户") << tr("客户SN") << tr("质检编号") << tr("返工编号") << tr("面板编号");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 8. 显示行数信息（可选）
    if (model->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("未找到符合条件的记录"));
    } else {
        QMessageBox::information(this, tr("成功"), tr("找到 %1 条记录").arg(model->rowCount()));
    }
}
void ExportTravels::on_comboBoxLine_currentTextChanged(const QString &arg1)
{
    if (arg1.isEmpty()) {
        return;
    }
    ManagerSajet::loadProcess(ui->comboBoxProcess,arg1);
}
void ExportTravels::on_comboBoxProcess_currentTextChanged(const QString &arg1)
{
    QString line = ui->comboBoxLine->currentText().trimmed();
    qDebug()<<line<<arg1;
    if (arg1.isEmpty()||line.isEmpty()) {
        return;
    }
    ManagerSajet::loadTerminal(ui->comboBoxTerminal,line,arg1);
}
void ExportTravels::on_pushButtonExport_clicked()
{
    // 1. 检查是否有模型
    QAbstractItemModel *model = ui->tableView->model();
    if (!model) {
        QMessageBox::warning(this, tr("提示"), tr("没有可导出的数据，请先查询"));
        return;
    }

    // 2. 让用户选择保存路径
    QString defaultFileName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    tr("导出 CSV"),
                                                    defaultFileName,
                                                    tr("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    // 3. 打开文件并写入
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法创建文件: %1").arg(filePath));
        return;
    }

    QTextStream out(&file);
    // 使用 UTF-8 with BOM，使 Excel 正确识别
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);

    // 4. 写入表头
    QStringList headerItems;
    for (int col = 0; col < model->columnCount(); ++col) {
        QString header = model->headerData(col, Qt::Horizontal).toString();
        if (header.isEmpty()) {
            header = QString("Column%1").arg(col + 1);
        }
        // 如果表头包含逗号、引号或换行，用双引号包裹
        if (header.contains(',') || header.contains('"') || header.contains('\n')) {
            header.replace("\"", "\"\"");
            header = "\"" + header + "\"";
        }
        headerItems << header;
    }
    out << headerItems.join(",") << "\n";

    // 5. 写入数据行
    for (int row = 0; row < model->rowCount(); ++row) {
        QStringList rowItems;
        for (int col = 0; col < model->columnCount(); ++col) {
            QModelIndex index = model->index(row, col);
            QString data = model->data(index).toString();
            // 处理包含特殊字符的字段
            if (data.contains(',') || data.contains('"') || data.contains('\n')) {
                data.replace("\"", "\"\"");
                data = "\"" + data + "\"";
            }
            rowItems << data;
        }
        out << rowItems.join(",") << "\n";
    }

    file.close();

    QMessageBox::information(this, tr("成功"), tr("导出完成: %1").arg(filePath));
}
