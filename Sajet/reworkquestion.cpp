#include "reworkquestion.h"
#include "ui_reworkquestion.h"
#include "oracle_manager.h"
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTextEdit>

ReworkQuestion::ReworkQuestion(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ReworkQuestion)
{
    ui->setupUi(this);
    update_table();
}

ReworkQuestion::~ReworkQuestion()
{
    delete ui;
}

void ReworkQuestion::update_table()
{
    // 1. 获取界面输入
    int floorIndex = ui->comboBox->currentIndex();
    QDateTime selectedDateTime = ui->dateTimeEdit->dateTime();
    QString key = ui->keylineEdit->text().trimmed();
    bool showUnfinished = ui->radioButton->isChecked();   // true: 仅显示未完成的 (CHECKED='Y')

    // 2. 楼层条件
    QStringList floorConditions;
    switch (floorIndex) {
    case 0: // 全部
        floorConditions << "R.PDLINE_NAME LIKE 'B2%'"
                        << "R.PDLINE_NAME LIKE 'B3%'"
                        << "R.PDLINE_NAME LIKE 'B4%'";
        break;
    case 1: floorConditions << "R.PDLINE_NAME LIKE 'B2%'"; break;
    case 2: floorConditions << "R.PDLINE_NAME LIKE 'B3%'"; break;
    case 3: floorConditions << "R.PDLINE_NAME LIKE 'B31%'"; break;
    case 4: floorConditions << "R.PDLINE_NAME LIKE 'B32%'"; break;
    case 5: floorConditions << "R.PDLINE_NAME LIKE 'B33%'"; break;
    case 6: floorConditions << "R.PDLINE_NAME LIKE 'B4%'"; break;
    case 7: floorConditions << "R.PDLINE_NAME LIKE 'B41%'"; break;
    case 8: floorConditions << "R.PDLINE_NAME LIKE 'B42%'"; break;
    case 9: floorConditions << "R.PDLINE_NAME LIKE 'B43%'"; break;
    default: break;
    }
    QString floorClause = floorConditions.isEmpty() ? "" : " AND (" + floorConditions.join(" OR ") + ")";

    // 3. 时间条件：用户选择时间 → 当前时间
    QDateTime startDateTime = selectedDateTime;
    if (!startDateTime.isValid()) {
        startDateTime = QDateTime::currentDateTime().addDays(-1);
    }
    QDateTime endDateTime = QDateTime::currentDateTime();
    QString startStr = startDateTime.toString("yyyy-MM-dd hh:mm:ss");
    QString endStr   = endDateTime.toString("yyyy-MM-dd hh:mm:ss");
    QString timeClause = QString(
                             " R.CREATEDATE >= TO_DATE('%1', 'YYYY-MM-DD HH24:MI:SS') "
                             " AND R.CREATEDATE <= TO_DATE('%2', 'YYYY-MM-DD HH24:MI:SS')"
                             ).arg(startStr, endStr);

    // 4. 未完成筛选
    QString unfinishedClause = showUnfinished ? " AND R.CHECKED = 'Y' AND R.CREATEDATE IS NOT NULL AND R.DUALTIME IS NULL" : "";

    // 5. 关键字筛选 (序列号 或 原因)
    QString keyClause;
    if (!key.isEmpty()) {
        QString safeKey = key;
        safeKey.replace("'", "''");
        keyClause = QString(" AND (R.SERIAL_NUMBER LIKE '%%1%' OR R.REASON LIKE '%%1%')").arg(safeKey);
    }

    // 6. 构建 SQL（使用子查询先排序，再取前50）
    QString sql = QString(
                      "SELECT * FROM ("
                      "SELECT R.NUMBERINDEX, R.PDLINE_NAME, R.SERIAL_NUMBER, "
                      "R.REASON, R.QUERY, R.QEMP, R.CREATEDATE, R.CEMP, "
                      "R.CHECKED, R.EMP_SFIS, R.REWORK_NO, R.DUALTIME "
                      "FROM SAJET.ECS_SN_REWORK R "
                      "WHERE %1 %2 %3 %4 "
                      "ORDER BY R.NUMBERINDEX DESC"
                      ") WHERE ROWNUM <= 50"
                      ).arg(timeClause, floorClause, unfinishedClause, keyClause);

    qDebug() << "Executing SQL:" << sql;

    // 7. 连接数据库
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qDebug() << "Database connection invalid";
        return;
    }

    // 8. 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qDebug() << "Query error:" << query.lastError().text();
        return;
    }

    // 9. 更新模型
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }
    m_model = new QSqlQueryModel(this);
    m_model->setQuery(std::move(query));

    if (m_model->lastError().isValid()) {
        qDebug() << "Model error:" << m_model->lastError().text();
        delete m_model;
        m_model = nullptr;
        return;
    }

    // 10. 设置自定义表头（与 SELECT 列顺序一致）
    QStringList headers;
    headers << tr("序号") << tr("产线") << tr("问题描述")
            << tr("原因") << tr("要求") << tr("提交人")
            << tr("创建时间") << tr("确认人") << tr("已确认")
            << tr("处理人") << tr("回复") << tr("完成时间");
    for (int i = 0; i < headers.size() && i < m_model->columnCount(); ++i) {
        m_model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 11. 显示到表格
    ui->tableView->setModel(m_model);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setMaximumSectionSize(250);
    ui->tableView->setWordWrap(true);
    // 垂直头（行号）也可以限制最大高度（可选）
    // ui->tableView->verticalHeader()->setMaximumSectionSize(60);
    ui->tableView->resizeColumnsToContents();
}
void ReworkQuestion::on_selectButton_clicked()
{
    update_table();
}
void ReworkQuestion::on_replyButton_clicked()
{
    // 1. 获取回复内容
    QString reply = ui->replylineEdit->text().trimmed();
    if (reply.isEmpty()) {
        reply = "OK";
    }

    // 2. 获取当前选中行
    QModelIndex currentIndex = ui->tableView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("选择错误"), tr("请先选择一条记录"));
        return;
    }

    // 获取该行的第一列（NUMBERINDEX）的数据
    QModelIndex idIndex = ui->tableView->model()->index(currentIndex.row(), 0);
    if (!idIndex.isValid()) {
        QMessageBox::warning(this, tr("数据错误"), tr("无法获取记录ID"));
        return;
    }
    QString recordId = ui->tableView->model()->data(idIndex).toString();
    if (recordId.isEmpty()) {
        QMessageBox::warning(this, tr("数据错误"), tr("记录ID为空"));
        return;
    }

    QString empNo = OracleManager::getCurrentUsername();
    if (empNo.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("无法获取当前用户信息"));
        return;
    }

    // 4. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }
    int ret = QMessageBox::question(this, tr("确认"),
                                    tr("确定要回复记录 %1 为 %2 吗？").arg(recordId).arg(reply),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    // 5. 构建 UPDATE SQL（使用参数绑定防止注入）
    QString sql = "UPDATE SAJET.ECS_SN_REWORK R "
                  "SET R.EMP_SFIS = (SELECT E.EMP_NAME FROM SAJET.SYS_EMP E WHERE E.EMP_NO = :emp_no), "
                  "R.DUALTIME = SYSDATE, "
                  "R.REWORK_NO = :reply "
                  "WHERE R.NUMBERINDEX = :record_id";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":emp_no", empNo);
    query.bindValue(":reply", reply);
    query.bindValue(":record_id", recordId);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("更新失败"), tr("数据库更新出错: %1").arg(query.lastError().text()));
        return;
    }

    // 6. 检查影响行数
    int affectedRows = query.numRowsAffected();
    if (affectedRows == 0) {
        QMessageBox::information(this, tr("更新结果"), tr("未找到对应的记录，更新失败"));
        return;
    }

    QMessageBox::information(this, tr("成功"), tr("已成功更新记录"));
    // 清空回复输入框
    ui->replylineEdit->clear();
    // 刷新表格（重新查询）
    update_table();
}

void ReworkQuestion::on_tableView_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QString cellText = ui->tableView->model()->data(index).toString();
    if (cellText.isEmpty()) return;

    // 创建对话框显示完整内容
    QDialog dialog(this);
    dialog.setWindowTitle(tr("详细内容"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setPlainText(cellText);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    QPushButton *closeBtn = new QPushButton(tr("关闭"), &dialog);
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}
