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
    QString timeStart = ui->dateTimeEditStart->dateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString timeEnd = ui->dateTimeEditEnd->dateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;

    }

    // 3. 动态构建 WHERE 条件
    QStringList whereClauses;
    m_bindValues.clear();
    QString lineId;
    // 工单
    if (!wo.isEmpty()) {
        whereClauses << "T.WORK_ORDER = :wo";
        m_bindValues[":wo"] = wo;
    }
    // 料号
    if (!part.isEmpty()) {
        whereClauses << "T.MODEL_ID = (SELECT PART_ID FROM SAJET.SYS_PART WHERE PART_NO = :part)";
        m_bindValues[":part"] = part;
    }
    // 产线
    if (!line.isEmpty()) {
        whereClauses << "T.PDLINE_ID =(SELECT PDLINE_ID FROM SAJET.SYS_PDLINE WHERE PDLINE_NAME = :line)";
        m_bindValues[":line"] = line;
    }
    // 工序
    if (!process.isEmpty()) {
        whereClauses << "T.PROCESS_ID = (SELECT PROCESS_ID FROM SAJET.SYS_PROCESS WHERE PROCESS_NAME = :processid)";
        m_bindValues[":processid"] = process;
    }
    // 终端
    if (!terminal.isEmpty()) {
        whereClauses << "T.TERMINAL_ID = (SELECT TERMINAL_ID FROM SAJET.SYS_TERMINAL WHERE TERMINAL_NAME = :terminal)";
        m_bindValues[":terminal"] = terminal;
    }
    // 时间范围
    if (!timeStart.isEmpty() && !timeEnd.isEmpty()) {
        whereClauses << "T.OUT_PROCESS_TIME BETWEEN TO_DATE(:startTime, 'YYYY-MM-DD HH24:MI:SS') AND TO_DATE(:endTime, 'YYYY-MM-DD HH24:MI:SS')";
        m_bindValues[":startTime"] = timeStart;
        m_bindValues[":endTime"] = timeEnd;
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
    qDebug()<<sql;

    // 5. 准备并执行查询
    QSqlQuery query(db);
    query.prepare(sql);
    for (auto it = m_bindValues.begin(); it != m_bindValues.end(); ++it) {
        qDebug()<<"bindValues:"<<it.key()<<it.value();
        query.bindValue(it.key(), it.value());
    }
    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }
    m_currentSql = sql;

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
            << tr("客户") << tr("客户SN") << tr("质检编号") << tr("返工编号") << tr("电压值");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    if (model->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("未找到符合条件的记录"));
    }
}
void ExportTravels::on_comboBoxLine_currentTextChanged(const QString &arg1)
{
    if (arg1.isEmpty()) {
        return;
    }
    ManagerSajet::loadProcess(ui->comboBoxProcess,arg1);
    ui->comboBoxProcess->setCurrentIndex(-1);
}
void ExportTravels::on_comboBoxProcess_currentTextChanged(const QString &arg1)
{
    QString line = ui->comboBoxLine->currentText().trimmed();
    if (arg1.isEmpty()||line.isEmpty()) {
        return;
    }
    ManagerSajet::loadTerminal(ui->comboBoxTerminal,line,arg1);
    ui->comboBoxTerminal->setCurrentIndex(-1);
}
void ExportTravels::on_pushButtonExport_clicked()
{
    QString result = OracleManager::exportSqlToCsv(m_currentSql,m_bindValues,ui->tableView);
    if (result.startsWith("OK:")) {
        QString filePath = result.mid(3);   // 去掉 "OK:"
        QMessageBox::information(this, tr("成功"), tr("导出成功: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, tr("错误"), tr("导出失败: %1").arg(result));
    }
}
