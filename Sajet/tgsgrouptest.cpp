#include "tgsgrouptest.h"
#include "ui_tgsgrouptest.h"
#include "oracle_manager.h"
#include "tgsgroup.h"
#include "managersajet.h"
#include <QMessageBox>

TGSGroupTest::TGSGroupTest(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGSGroupTest)
{
    ui->setupUi(this);
    ManagerSajet::loadPDline(ui->comboBoxLine);
}

TGSGroupTest::~TGSGroupTest()
{
    delete ui;
}
void TGSGroupTest::on_comboBoxLine_currentTextChanged(const QString &arg1)
{
    if (arg1.trimmed().isEmpty()) {
        ui->comboBoxProcess->clear();
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT DISTINCT PR.PROCESS_NAME "
                  "FROM SAJET.SYS_TERMINAL T "
                  "JOIN SAJET.SYS_PROCESS PR ON PR.PROCESS_ID = T.PROCESS_ID "
                  "WHERE T.PDLINE_ID = (SELECT P.PDLINE_ID FROM SAJET.SYS_PDLINE P WHERE P.PDLINE_NAME = :line) "
                  "AND T.ENABLED = 'Y' "
                  "ORDER BY PR.PROCESS_NAME";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":line", arg1);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询工序失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->comboBoxProcess->clear();
    while (query.next()) {
        ui->comboBoxProcess->addItem(query.value(0).toString());
    }

    if (ui->comboBoxProcess->count() == 0) {
        ui->comboBoxProcess->addItem(tr("无工序"));
    }
}

void TGSGroupTest::on_comboBoxProcess_currentTextChanged(const QString &arg1)
{
    if (arg1.trimmed().isEmpty() || ui->comboBoxLine->currentText().trimmed().isEmpty()) {
        ui->comboBoxName->clear();
        return;
    }

    QString lineName = ui->comboBoxLine->currentText().trimmed();

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT T.TERMINAL_ID, T.TERMINAL_NAME, T.PDLINE_ID, T.PROCESS_ID, T.STAGE_ID "
                  "FROM SAJET.SYS_TERMINAL T "
                  "JOIN SAJET.SYS_PROCESS PR ON PR.PROCESS_ID = T.PROCESS_ID "
                  "WHERE T.PDLINE_ID = (SELECT P.PDLINE_ID FROM SAJET.SYS_PDLINE P WHERE P.PDLINE_NAME = :line) "
                  "AND T.ENABLED = 'Y' AND PR.PROCESS_NAME = :process "
                  "ORDER BY T.TERMINAL_NAME";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":line", lineName);
    query.bindValue(":process", arg1);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询终端失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->comboBoxName->clear();
    // 添加一个默认空项（可选）
    ui->comboBoxName->addItem(tr("请选择终端"));

    while (query.next()) {
        QString terminalName = query.value("TERMINAL_NAME").toString();
        // 将其他信息存储为 UserData (QVariantMap)
        QVariantMap data;
        data["TERMINAL_ID"] = query.value("TERMINAL_ID").toString();
        data["TERMINAL_NAME"] = terminalName;
        data["PDLINE_ID"] = query.value("PDLINE_ID").toString();
        data["PROCESS_ID"] = query.value("PROCESS_ID").toString();
        data["STAGE_ID"] = query.value("STAGE_ID").toString();

        ui->comboBoxName->addItem(terminalName, QVariant::fromValue(data));
    }

    if (ui->comboBoxName->count() == 0) {
        ui->comboBoxName->addItem(tr("无终端"));
    }
}
void TGSGroupTest::on_lineEditGroupID_returnPressed()
{
    bool ok;
    int id = ui->lineEditGroupID->text().toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的数字"));
        return;
    }

    int index = ui->comboBoxName->currentIndex();
    if (index < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请选择终端"));
        return;
    }

    QVariant var = ui->comboBoxName->itemData(index);
    if (!var.canConvert<QVariantMap>()) {
        QMessageBox::warning(this, tr("错误"), tr("终端数据无效"));
        return;
    }

    QVariantMap data = var.toMap();
    m_dataMap.clear();
    m_dataMap["TGROUPID"] = QString::number(id);
    m_dataMap["TTERMINALID"]   = data["TERMINAL_ID"].toString();
    m_dataMap["TTERMINALNAME"] = data["TERMINAL_NAME"].toString();
    m_dataMap["TPDLINEID"]     = data["PDLINE_ID"].toString();
    m_dataMap["TPROCESSID"]    = data["PROCESS_ID"].toString();
    m_dataMap["TSTAGEID"]      = data["STAGE_ID"].toString();

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT T.GROUP_ID,T.GROUP_DESC_E,G.JOB_ID,G.GROUP_SEQ, B.TYPE_NAME_E, B.PROC_CALL_NAME, "
                  "J.JOB_DESC_E,G.SEQ_ELSE, G.SEQ_OTHER, G.VALUE_KIND,J.TYPE_ID  "
                  "FROM SAJET.TGS_GROUP_BASE T "
                  "INNER JOIN SAJET.TGS_GROUP_LINK G ON G.GROUP_ID = T.GROUP_ID "
                  "INNER JOIN SAJET.TGS_JOB_BASE J ON J.JOB_ID = G.JOB_ID "
                  "INNER JOIN SAJET.TGS_JOB_TYPE_BASE B ON B.TYPE_ID = J.TYPE_ID "
                  "WHERE T.GROUP_ID = :id "
                  "ORDER BY G.GROUP_SEQ";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":id", id);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    m_groupJobList.clear();
    bool first = true;
    while (query.next()) {
        GroupJobInfo info;
        info.groupId      = query.value("GROUP_ID").toString();
        info.groupName    = query.value("GROUP_DESC_E").toString();
        info.jobId        = query.value("JOB_ID").toString();
        info.jobDesc      = query.value("JOB_DESC_E").toString();
        info.typeNameE    = query.value("TYPE_NAME_E").toString();
        info.procCallName = query.value("PROC_CALL_NAME").toString();
        info.groupSeq     = query.value("GROUP_SEQ").toString();
        info.seqElse      = query.value("SEQ_ELSE").toString();
        info.seqOther     = query.value("SEQ_OTHER").toString();
        info.valueKind    = query.value("VALUE_KIND").toString();
        info.typeId       = query.value("TYPE_ID").toString();
        m_groupJobList.append(info);

        if (first) {
            ui->plainTextEditGroup->appendPlainText(
                QString("[Current Execute Group: %1-%2]").arg(info.groupId).arg(info.groupName)
                );
            ui->labelType->setText(info.typeNameE);
            first = false;
        }
        ui->plainTextEditGroup->appendPlainText(
            QString("[Will Execute Job: %1-%2 (Step: %3); ProcCallName: %4; TypeName: %5]")
                .arg(info.jobId)
                .arg(info.jobDesc)
                .arg(info.groupSeq)
                .arg(info.procCallName)
                .arg(info.typeNameE)
            );
    }
    if (m_groupJobList.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("未找到匹配的记录"));
        return;
    }
    m_maxStep = m_groupJobList.size();
    m_currentStep = 0;
}
void TGSGroupTest::fetchJobDetails(QString groupid, QString jobid)
{
    m_jobDetailList.clear();

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT L.JOB_SEQ, L.SPROC_NAME "
                  "FROM SAJET.TGS_GROUP_BASE T "
                  "INNER JOIN SAJET.TGS_GROUP_LINK G ON G.GROUP_ID = T.GROUP_ID "
                  "INNER JOIN SAJET.TGS_JOB_BASE J ON J.JOB_ID = G.JOB_ID "
                  "INNER JOIN SAJET.TGS_JOB_LINK L ON L.JOB_ID = J.JOB_ID "
                  "WHERE T.GROUP_ID = :groupid AND J.JOB_ID = :jobid "
                  "ORDER BY L.JOB_SEQ";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":groupid", groupid);
    query.bindValue(":jobid", jobid);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"),
                              tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        JobDetailInfo detail;
        detail.jobSeq    = query.value(0).toString();
        detail.sprocName = query.value(1).toString();
        m_jobDetailList.append(detail);
    }
}
void TGSGroupTest::executeJobProc(QString TREV,QString procCallname)
{
    if (m_jobDetailList.isEmpty()) {
        ui->plainTextEditGroup->appendPlainText("[ERROR][Execute JobProc: No procedures to execute]");
        return;
    }

    ui->plainTextEditGroup->appendPlainText("[INFO][JOB START]");
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        ui->plainTextEditGroup->appendPlainText("[ERROR][Database connection invalid]");
        return;
    }
    m_dataMap[procCallname] = TREV;
    ui->plainTextEditGroup->appendPlainText("[INFO][INPUT("+TREV+") SAVE TO "+procCallname+"]");
    for (const JobDetailInfo &jobInfo : m_jobDetailList) {
        QString procName = jobInfo.sprocName;

        if (procName.isEmpty()) {
            ui->plainTextEditGroup->appendPlainText("[WARNING][Procedure name is empty, skipped]");
            continue;
        }

        ui->plainTextEditGroup->appendPlainText(
            QString("[INFO][Start Execute: %1]").arg(procName)
            );
        ui->plainTextEditGroup->appendPlainText(
            QString("[INFO][TREV input: %1]").arg(TREV)
            );

        // 1. 获取存储过程参数信息
        ProcParams params = TGSGroup::getProcedureParams(procName);
        if (params.inParams.isEmpty() && params.outParams.isEmpty()) {
            ui->plainTextEditGroup->appendPlainText(
                QString("[WARNING][%1] No parameter info (insufficient privileges or not a procedure)").arg(procName)
                );
            continue;
        }

        // 2. 构建存储过程调用
        QString owner, name;
        if (procName.contains('.')) {
            QStringList parts = procName.split('.');
            owner = parts[0].trimmed();
            name = parts[1].trimmed();
        } else {
            owner = OracleManager::instance().getCurrentUsername();
            name = procName.trimmed();
        }

        int totalParams = params.inParams.size() + params.outParams.size();
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

        // 3. 绑定输入参数（值来自 m_dataMap 或 TREV）
        int idx = 0;
        QMap<QString, QString> inParamValues; // 存储参数名和值（字符串形式），用于日志

        for (const QString &param : params.inParams) {
            QVariant bindValue;
            QString logValue;

            if (param == "TREV") {
                bindValue = TREV;
                logValue = TREV;
            } else if (param == "TNOW") {
                QDateTime now = QDateTime::currentDateTime();
                bindValue = now;
                logValue = now.toString("yyyy-MM-dd hh:mm:ss");
            } else {
                QString val = m_dataMap.value(param, "");
                bindValue = val;
                logValue = val;
            }

            inParamValues[param] = logValue;
            query.bindValue(QString(":p%1").arg(idx), bindValue);
            idx++;
        }

        // 4. 绑定输出参数（预分配缓冲区）
        QList<QString> outBuffers;
        for (int i = 0; i < params.outParams.size(); ++i) {
            QString buffer;
            buffer.reserve(4000);
            outBuffers.append(buffer);
            query.bindValue(QString(":p%1").arg(idx), buffer, QSql::Out);
            idx++;
        }

        // 5. 执行存储过程
        if (!query.exec()) {
            ui->plainTextEditGroup->appendPlainText(
                QString("[ERROR][Execute failed: %1]").arg(query.lastError().text())
                );
            continue;
        }

        // 6. 获取输出参数值
        QString tresValue;  // 用于存储 TRES 的值
        QStringList outParamValues;
        idx = params.inParams.size();
        for (int i = 0; i < params.outParams.size(); ++i) {
            QString value = query.boundValue(QString(":p%1").arg(idx)).toString().trimmed();
            if (params.outParams[i] == "TRES") {
                tresValue = value;
            }
            outParamValues << QString("%1=%2").arg(params.outParams[i]).arg(value.isEmpty() ? "空" : value);
            idx++;
        }
        if (tresValue == "OK") {
            ui->plainTextEditGroup->appendPlainText(
                QString("[INFO][TRES: %1]").arg(tresValue)
                );
        }else{
            ui->plainTextEditGroup->appendPlainText(
                QString("[ERROR][TRES: %1]").arg(tresValue)
                );
        }

        // 7. 构造输入参数日志（显示参数名=值）
        QStringList inParamLog;
        for (const QString &param : params.inParams) {
            QString val = inParamValues[param];
            inParamLog << QString("%1=%2").arg(param).arg(val.isEmpty() ? "空" : val);
        }

        // 8. 输出执行结果
        QString log = QString("[INFO][Procedure: %1 | IN: %2]")
                          .arg(procName)
                          .arg(inParamLog.join(", "));
        if (!outParamValues.isEmpty()) {
            log += QString(" | OUT: %1").arg(outParamValues.join(", "));
            log += QString("]");
        }
        ui->plainTextEditGroup->appendPlainText(log);
    }
    ui->plainTextEditGroup->appendPlainText("[INFO][JOB COMPLETE]");
    ui->plainTextEditGroup->appendPlainText("");
}
void TGSGroupTest::on_lineEditTrev_returnPressed()
{
    if (m_groupJobList.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先查询数据"));
        return;
    }
    if (m_currentStep >= m_maxStep) {
        QMessageBox::information(this, tr("完成"), tr("所有步骤已执行完毕"));
        return;
    }
    const GroupJobInfo &jobInfo = m_groupJobList[m_currentStep];
    QString input = ui->lineEditTrev->text().trimmed();

    ui->plainTextEditGroup->appendPlainText(QString("[EXECUTE %1/%2: JOB_ID=%3, TREV=%4]")
                                                .arg(m_currentStep + 1)
                                                .arg(m_maxStep)
                                                .arg(jobInfo.jobId)
                                                .arg(input.isEmpty() ? "(空)" : input));
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_dataMap["TNOW"] = currentTime;
    ui->plainTextEditGroup->appendPlainText(QString("[INFO][INPUT(%1) SAVE TO TNOW]").arg(currentTime));
    fetchJobDetails(jobInfo.groupId, jobInfo.jobId);
    executeJobProc(input,jobInfo.procCallName);
    ui->lineEditTrev->clear();
    m_currentStep++;
    if (m_currentStep >= m_maxStep) {
        QMessageBox::information(this, tr("完成"), tr("所有步骤已执行完毕"));
        return;
    }
    const GroupJobInfo &nextJob = m_groupJobList[m_currentStep];
    ui->labelType->setText(nextJob.typeNameE);
}
void TGSGroupTest::on_comboBoxName_currentIndexChanged(int index)
{
    if (index < 0) {
        ui->plainTextEditGroup->appendPlainText(tr("[Terminal selection cleared]"));
        return;
    }

    QVariant var = ui->comboBoxName->itemData(index);
    if (!var.canConvert<QVariantMap>()) {
        ui->plainTextEditGroup->appendPlainText(tr("[Terminal Data Empty]"));
        return;
    }

    QVariantMap data = var.toMap();
    QString terminalId   = data["TERMINAL_ID"].toString();
    QString terminalName = data["TERMINAL_NAME"].toString();
    QString pdlineId     = data["PDLINE_ID"].toString();
    QString processId    = data["PROCESS_ID"].toString();
    QString stageId      = data["STAGE_ID"].toString();

    ui->plainTextEditGroup->appendPlainText(
        QString("[TERMINAL CHANGED] TTERMINALID: %1 | TTERMINALNAME: %2 | TPDLINEID: %3 | TPROCESSID: %4 | TSTAGEID: %5]")
            .arg(terminalId)
            .arg(terminalName)
            .arg(pdlineId)
            .arg(processId)
            .arg(stageId)
        );
}