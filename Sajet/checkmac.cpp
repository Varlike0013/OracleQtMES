#include "checkmac.h"
#include "ui_checkmac.h"
#include "managersajet.h"
#include <QMessageBox>
#include <qsqlquery.h>
#include <qsqlquerymodel.h>

QMap<QString, QStringList> CheckMac::m_conditions;

CheckMac::CheckMac(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CheckMac)
{
    ui->setupUi(this);
}

CheckMac::~CheckMac()
{
    delete ui;
}

void CheckMac::on_lineEditInput_returnPressed()
{
    int index = ui->comboBoxInput->currentIndex();
    QString text = ui->lineEditInput->text().trimmed();

    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("不能为空"));
        return;
    }

    // 确保树有两列...
    if (ui->treeWidgetInput->columnCount() < 2) {
        ui->treeWidgetInput->setColumnCount(2);
        ui->treeWidgetInput->setHeaderLabels(QStringList() << tr("条件") << tr("值"));
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
    } else if (index == 1) { // 重工号
        if (ManagerSajet::is_ReworkNo(text)) {
            key = "REWORK";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("重工号不存在"));
        }
    } else {
        QMessageBox::warning(this, tr("未知错误"), tr("未知错误"));
    }

    if (valid) {
        // 1. 添加到树控件
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidgetInput);
        item->setText(0, key);
        item->setText(1, text);
        ui->treeWidgetInput->addTopLevelItem(item);
        ui->treeWidgetInput->expandAll();

        // 2. 同步添加到全局字典
        addCondition(key, text);

        // 3. 清空输入框
        ui->lineEditInput->clear();
        UpadteTableMacs();
    }
}
void CheckMac::addCondition(const QString &key, const QString &value)
{
    if (key.isEmpty() || value.isEmpty()) return;
    // 如果该键已存在，追加值；否则创建新列表
    if (m_conditions.contains(key)) {
        if (!m_conditions[key].contains(value)) {  // 避免重复添加
            m_conditions[key].append(value);
        }
    } else {
        m_conditions[key] = QStringList() << value;
    }
}

bool CheckMac::removeCondition(const QString &key, const QString &value)
{
    if (!m_conditions.contains(key)) return false;
    bool removed = m_conditions[key].removeOne(value);
    if (m_conditions[key].isEmpty()) {
        m_conditions.remove(key);
    }
    return removed;
}

void CheckMac::clearConditions()
{
    m_conditions.clear();
}

QStringList CheckMac::getConditionValues(const QString &key)
{
    return m_conditions.value(key, QStringList());
}

QMap<QString, QStringList> CheckMac::getAllConditions()
{
    return m_conditions;
}

bool CheckMac::conditionExists(const QString &key, const QString &value)
{
    return m_conditions.contains(key) && m_conditions[key].contains(value);
}

void CheckMac::on_pushButton_clicked()
{
    ui->treeWidgetInput->clear();
    clearConditions();
}
void CheckMac::UpadteTableMacs()
{
    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWORK");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && reworks.isEmpty()) {
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

    QString subSql = "SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE " + subConditions.join(" OR ");

    // 5. 完整查询语句
    QString sql = "SELECT M.WORK_ORDER, M.SERIAL_NUMBER, M.MAC, "
                  "NVL(P.PROCESS_NAME, 'None') AS CURRENT_PROCESS, "
                  "E.EMP_NAME, M.UPDATE_TIME, M.UUID, M.CUSTOMER_SN "
                  "FROM SAJET.G_WO_MAC M "
                  "LEFT JOIN SAJET.G_SN_STATUS S ON S.SERIAL_NUMBER = M.SERIAL_NUMBER "
                  "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = M.UPDATE_USERID "
                  "LEFT JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = S.WIP_PROCESS "
                  "WHERE M.SERIAL_NUMBER IN (" + subSql + ")";

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

    QStringList headers;
    headers << tr("工单") << tr("序列号") << tr("MAC地址")
            << tr("当前工序") << tr("更新用户") << tr("更新时间")
            << tr("UUID") << tr("客户SN");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }
    // 8. 设置到表格
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void CheckMac::on_pushButtonDelete_clicked()
{
    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWORK");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && reworks.isEmpty()) {
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

    QString subSql = "SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE S.WIP_PROCESS = '200018' AND (" + subConditions.join(" OR ") + ")";
    QString sql_backup = "INSERT INTO SAJET.G_HT_WO_MAC SELECT * FROM SAJET.G_WO_MAC WHERE SERIAL_NUMBER IN (" + subSql + ")";
    QString sql_delete = "DELETE FROM SAJET.G_WO_MAC WHERE SERIAL_NUMBER IN (" + subSql + ")";

    qDebug() << "Backup SQL:" << sql_backup;
    qDebug() << "Delete SQL:" << sql_delete;

    if (!db.transaction()) {
        QMessageBox::critical(this, tr("错误"), tr("启动事务失败: %1").arg(db.lastError().text()));
        return;
    }

    QSqlQuery query(db);
    if (!query.exec(sql_backup)) {
        db.rollback();
        QMessageBox::critical(this, tr("错误"), tr("备份数据失败: %1").arg(query.lastError().text()));
        return;
    }

    if (!query.exec(sql_delete)) {
        db.rollback();
        QMessageBox::critical(this, tr("错误"), tr("删除数据失败: %1").arg(query.lastError().text()));
        return;
    }

    // 提交事务
    if (!db.commit()) {
        QMessageBox::critical(this, tr("错误"), tr("提交事务失败: %1").arg(db.lastError().text()));
        return;
    }
    QMessageBox::information(this, tr("成功"), tr("备份并删除成功"));
    UpadteTableMacs(); // 或者重新查询显示
}

