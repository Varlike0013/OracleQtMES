#include "tgsgroup.h"
#include "ui_tgsgroup.h"
#include "oracle_manager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QGroupBox>
#include <QFormLayout>

TGSGroup::TGSGroup(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGSGroup)
{
    ui->setupUi(this);
}

TGSGroup::~TGSGroup()
{
    delete ui;
}
void TGSGroup::on_lineEditInput_returnPressed()
{
    int type = ui->comboBoxInput->currentIndex(); // 0:id 1:desc
    QString input = ui->lineEditInput->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入查询内容"));
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString whereClause;
    if (type == 0) {
        whereClause = "T.GROUP_ID = :input";
    } else {
        whereClause = "T.GROUP_DESC_E = :input";
    }

    QString sql = QString(
                      "SELECT T.GROUP_ID, T.GROUP_DESC_E, T.GROUP_DESC_C, T.ENABLED, "
                      "G.JOB_ID, B.TYPE_NAME_E, B.PROC_CALL_NAME, G.GROUP_SEQ, G.SEQ_ELSE, G.SEQ_OTHER, G.VALUE_KIND, "
                      "J.JOB_DESC_E, J.JOB_DESC_C, J.TYPE_ID, J.ENABLED, "
                      "L.JOB_SEQ, L.SPROC_NAME "
                      "FROM SAJET.TGS_GROUP_BASE T "
                      "INNER JOIN SAJET.TGS_GROUP_LINK G ON G.GROUP_ID = T.GROUP_ID "
                      "INNER JOIN SAJET.TGS_JOB_BASE J ON J.JOB_ID = G.JOB_ID "
                      "INNER JOIN SAJET.TGS_JOB_LINK L ON L.JOB_ID = J.JOB_ID "
                      "INNER JOIN SAJET.TGS_JOB_TYPE_BASE B ON B.TYPE_ID = J.TYPE_ID "
                      "WHERE %1 "
                      "ORDER BY G.GROUP_SEQ, L.JOB_SEQ"
                      ).arg(whereClause);

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":input", input);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->treeWidget->clear();
    QString firstGroupId, firstGroupDesc;
    int rowCount = 0;
    while (query.next()) {
        if (firstGroupId.isEmpty()) {
            firstGroupId = query.value("GROUP_ID").toString();
            firstGroupDesc = query.value("GROUP_DESC_E").toString();
        }
        QString jobId = query.value("JOB_ID").toString();
        QString typeNameE = query.value("TYPE_NAME_E").toString();
        QString procCallName = query.value("PROC_CALL_NAME").toString();
        QString sprocName = query.value("SPROC_NAME").toString();

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, jobId);
        item->setText(1, typeNameE);
        item->setText(2, procCallName);
        item->setText(3, sprocName);
        item->setData(0, Qt::UserRole, sprocName);
        rowCount++;
    }

    if (!firstGroupId.isEmpty()) {
        ui->labelGroupId->setText(firstGroupId);
        ui->labelGroupName->setText(firstGroupDesc);
    } else {
        ui->labelGroupId->setText(tr("ID"));
        ui->labelGroupName->setText(tr("未找到对应行为"));
    }
}

void TGSGroup::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;
    m_currentProc = item->data(0, Qt::UserRole).toString();
    if (m_currentProc .isEmpty()) {
        ui->plainTextEdit->setPlainText(tr("未关联存储过程"));
        return;
    }
    QString source = getProcedureSource(m_currentProc );
    ui->plainTextEdit->setPlainText(source);
}
QString TGSGroup::getProcedureSource(const QString &procName)
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        return tr("数据库连接无效");
    }

    // 解析 Schema 和存储过程名称
    QString owner, name;
    if (procName.contains('.')) {
        QStringList parts = procName.split('.');
        owner = parts[0].trimmed();
        name = parts[1].trimmed();
    } else {
        return tr("存储过程解析失败：")+procName;
    }

    // 查询 ALL_SOURCE（需有权限）
    QString sql = "SELECT TEXT FROM ALL_SOURCE "
                  "WHERE OWNER = :owner AND NAME = :name AND TYPE = 'PROCEDURE' "
                  "ORDER BY LINE";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":owner", owner);
    query.bindValue(":name", name);

    if (!query.exec()) {
        return tr("查询存储过程代码失败: %1").arg(query.lastError().text());
    }

    QString source;
    while (query.next()) {
        source += query.value(0).toString();
    }

    if (source.isEmpty()) {
        return tr("未找到存储过程 %1 的源代码（可能权限不足或名称错误）").arg(procName);
    }

    return source;
}
ProcParams TGSGroup::getProcedureParams(const QString &procName)
{
    ProcParams result;
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        return result;
    }

    // 解析 Schema 和存储过程名称
    QString owner, name;
    if (procName.contains('.')) {
        QStringList parts = procName.split('.');
        owner = parts[0].trimmed();
        name = parts[1].trimmed();
    } else {
        return result;
    }

    // 查询 ALL_ARGUMENTS（需有权限）
    QString sql = "SELECT ARGUMENT_NAME, IN_OUT FROM ALL_ARGUMENTS "
                  "WHERE OWNER = :owner AND OBJECT_NAME = :name "
                  "AND PACKAGE_NAME IS NULL "
                  "AND ARGUMENT_NAME IS NOT NULL "
                  "AND POSITION > 0 "
                  "ORDER BY POSITION";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":owner", owner);
    query.bindValue(":name", name);

    if (!query.exec()) {
        qWarning() << "查询参数失败:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QString argName = query.value(0).toString().toUpper().trimmed();
        QString inOut = query.value(1).toString().toUpper().trimmed();
        if (inOut == "IN") {
            result.inParams.append(argName);
        } else if (inOut == "OUT") {
            result.outParams.append(argName);
        }
    }

    return result;
}
void TGSGroup::on_pushButtonBuild_clicked()
{
    if (m_currentProc.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个存储过程"));
        return;
    }

    ProcParams params = getProcedureParams(m_currentProc);
    if (params.inParams.isEmpty() && params.outParams.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("未获取到参数信息，请检查权限"));
        return;
    }

    // 清空 frameTest 的旧内容
    m_inputEdits.clear();
    QLayout *oldLayout = ui->frameTest->layout();
    if (oldLayout) {
        while (oldLayout->count() > 0) {
            QLayoutItem *item = oldLayout->takeAt(0);
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        delete oldLayout;
    }
    QVBoxLayout *mainLayout = new QVBoxLayout(ui->frameTest);
    ui->frameTest->setLayout(mainLayout);

    // 输入区域
    QGroupBox *inputGroup = new QGroupBox(tr("输入参数"), ui->frameTest);
    QFormLayout *inputLayout = new QFormLayout(inputGroup);
    m_inputEdits.clear();
    for (const QString &param : params.inParams) {
        QLineEdit *edit = new QLineEdit(inputGroup);
        edit->setObjectName(param);
        inputLayout->addRow(param, edit);
        m_inputEdits[param] = edit;
    }
    mainLayout->addWidget(inputGroup);

    // 输出区域
    QGroupBox *outputGroup = new QGroupBox(tr("输出结果"), ui->frameTest);
    QFormLayout *outputLayout = new QFormLayout(outputGroup);
    for (const QString &param : params.outParams) {
        QLineEdit *edit = new QLineEdit(outputGroup);
        edit->setReadOnly(true);
        edit->setObjectName(param);
        outputLayout->addRow(param, edit);
    }
    mainLayout->addWidget(outputGroup);
    mainLayout->addStretch();
}
void TGSGroup::on_pushButtonExecute_clicked()
{
    if (m_currentProc.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("未选择存储过程"));
        return;
    }

    // 收集输入参数（从界面控件获取）
    QMap<QString, QString> inValues;
    for (auto it = m_inputEdits.begin(); it != m_inputEdits.end(); ++it) {
        inValues[it.key()] = it.value()->text().trimmed();
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 解析存储过程名
    QString owner, name;
    if (m_currentProc.contains('.')) {
        QStringList parts = m_currentProc.split('.');
        owner = parts[0].trimmed();
        name = parts[1].trimmed();
    } else {
        QMessageBox::warning(this, tr("提示"), tr("存储过程解析失败"));
        return;
    }

    // 获取参数列表
    ProcParams params = getProcedureParams(m_currentProc);
    int totalParams = params.inParams.size() + params.outParams.size();

    // 构建占位符
    QStringList placeholders;
    for (int i = 0; i < totalParams; ++i) {
        placeholders << QString(":p%1").arg(i);
    }
    QString procCall = QString("BEGIN %1.%2(%3); END;")
                           .arg(owner)
                           .arg(name)
                           .arg(placeholders.join(", "));

    QSqlQuery query(db);
    query.prepare(procCall);

    // 绑定 IN 参数
    int idx = 0;
    for (const QString &param : params.inParams) {
        query.bindValue(QString(":p%1").arg(idx), inValues.value(param, ""));
        idx++;
    }

    // 绑定 OUT 参数（仅用于占位，不依赖变量更新）
    idx = params.inParams.size();
    for (int i = 0; i < params.outParams.size(); ++i) {
        // 绑定一个临时变量，但不保存引用
        QString dummy;
        dummy.reserve(4000);
        query.bindValue(QString(":p%1").arg(idx), dummy, QSql::Out);
        idx++;
    }

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("调用存储过程失败: %1").arg(query.lastError().text()));
        return;
    }

    // 直接通过 boundValue 获取输出
    idx = params.inParams.size();
    for (int i = 0; i < params.outParams.size(); ++i) {
        QString value = query.boundValue(QString(":p%1").arg(idx)).toString().trimmed();
        QLineEdit *outEdit = ui->frameTest->findChild<QLineEdit*>(params.outParams[i]);
        if (outEdit) {
            outEdit->setText(value);
        }
        idx++;
    }
}
void TGSGroup::highlightAllMatches(const QString &text)
{
    // 清空之前的高亮
    QList<QTextEdit::ExtraSelection> extraSelections;
    ui->plainTextEdit->setExtraSelections(extraSelections);
    m_matchPositions.clear();
    m_currentMatchIndex = -1;

    if (text.isEmpty()) {
        return;
    }

    QTextDocument *doc = ui->plainTextEdit->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    // 查找所有匹配
    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = doc->find(text, cursor);
        if (!cursor.isNull()) {
            m_matchPositions.append(cursor);
        }
    }

    if (m_matchPositions.isEmpty()) {
        return;
    }

    // 高亮所有匹配项
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(Qt::yellow);
    for (const QTextCursor &pos : m_matchPositions) {
        selection.cursor = pos;
        extraSelections.append(selection);
    }
    ui->plainTextEdit->setExtraSelections(extraSelections);
    m_currentMatchIndex = 0;
    ui->plainTextEdit->setTextCursor(m_matchPositions[0]);
    ui->plainTextEdit->ensureCursorVisible();
}
void TGSGroup::on_lineEditQuery_returnPressed()
{
    QString input = ui->lineEditQuery->text().trimmed();
    highlightAllMatches(input);
}
void TGSGroup::on_pushButtonPrevious_clicked()
{
    if (m_matchPositions.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matchPositions.size()) % m_matchPositions.size();
    ui->plainTextEdit->setTextCursor(m_matchPositions[m_currentMatchIndex]);
    ui->plainTextEdit->ensureCursorVisible();
}
void TGSGroup::on_pushButtonNext_clicked()
{
    if (m_matchPositions.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_matchPositions.size();
    ui->plainTextEdit->setTextCursor(m_matchPositions[m_currentMatchIndex]);
    ui->plainTextEdit->ensureCursorVisible();
}
void TGSGroup::on_lineEditProc_returnPressed()
{
    m_currentProc = ui->lineEditProc->text().trimmed();
    if (m_currentProc .isEmpty()) {
        ui->plainTextEdit->setPlainText(tr("未关联存储过程"));
        return;
    }
    QString source = getProcedureSource(m_currentProc );
    ui->plainTextEdit->setPlainText(source);
}

