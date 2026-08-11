#include "clearkeyparts.h"
#include "ui_clearkeyparts.h"
#include "managersajet.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QStandardItem>

QMap<QString, QStringList> ClearKeyparts::m_conditions;

ClearKeyparts::ClearKeyparts(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClearKeyparts)
{
    ui->setupUi(this);
}

ClearKeyparts::~ClearKeyparts()
{
    delete ui;
}
void ClearKeyparts::addCondition(const QString &key, const QString &value)
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

bool ClearKeyparts::removeCondition(const QString &key, const QString &value)
{
    if (!m_conditions.contains(key)) return false;
    bool removed = m_conditions[key].removeOne(value);
    if (m_conditions[key].isEmpty()) {
        m_conditions.remove(key);
    }
    return removed;
}

void ClearKeyparts::clearConditions()
{
    m_conditions.clear();
}

QStringList ClearKeyparts::getConditionValues(const QString &key)
{
    return m_conditions.value(key, QStringList());
}

QMap<QString, QStringList> ClearKeyparts::getAllConditions()
{
    return m_conditions;
}

bool ClearKeyparts::conditionExists(const QString &key, const QString &value)
{
    return m_conditions.contains(key) && m_conditions[key].contains(value);
}
void ClearKeyparts::on_lineEditInput_returnPressed()
{
    int index = ui->comboBoxInput->currentIndex();
    QString input = ui->lineEditInput->text().trimmed();

    if (input.isEmpty()) {
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
        if (ManagerSajet::is_SERIAL_NUMBER(input)) {
            key = "SN";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("序列号不存在"));
        }
    } else if (index == 1) { // 重工号
        if (ManagerSajet::is_ReworkNo(input)) {
            key = "REWOWK";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("序列号不存在"));
        }
    } else {
        QMessageBox::warning(this, tr("未知错误"), tr("未知错误"));
    }

    if (valid) {
        // 1. 添加到树控件
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, key);
        item->setText(1, input);
        ui->treeWidget->addTopLevelItem(item);
        ui->treeWidget->expandAll();

        // 2. 同步添加到全局字典
        addCondition(key, input);

        // 3. 清空输入框
        ui->lineEditInput->clear();
        UpadteTableRow();
    }
}
void ClearKeyparts::UpadteTableRow()
{
    // 1. 获取条件
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWOWK");  // 注意键名拼写

    if (serials.isEmpty() && reworks.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加序列号或其他条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 2. 构建子查询条件（转义单引号）
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

    if (subConditions.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请添加查询条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 3. 构建子查询（获取符合条件的 SERIAL_NUMBER）
    QString subSql = "SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE " + subConditions.join(" OR ");

    // 4. 完整查询（10个字段）
    // QString sql = "SELECT K.WORK_ORDER, K.SERIAL_NUMBER, P.PROCESS_NAME, PA.PART_NO, PA.PART_TYPE, "
    //               "K.ITEM_PART_SN, K.ITEM_GROUP, K.VERSION, E.EMP_NAME, K.UPDATE_TIME "
    //               "FROM SAJET.G_SN_KEYPARTS K "
    //               "LEFT JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = K.PROCESS_ID "
    //               "LEFT JOIN SAJET.SYS_PART PA ON PA.PART_ID = K.ITEM_PART_ID "
    //               "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = K.UPDATE_USERID "
    //               "WHERE K.SERIAL_NUMBER IN (" + subSql + ")";
    QString sql = "SELECT DISTINCT K.WORK_ORDER, P.PROCESS_NAME "
                  "FROM SAJET.G_SN_KEYPARTS K "
                  "LEFT JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = K.PROCESS_ID "
                  "LEFT JOIN SAJET.SYS_PART PA ON PA.PART_ID = K.ITEM_PART_ID "
                  "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = K.UPDATE_USERID "
                  "WHERE K.SERIAL_NUMBER IN (" + subSql + ") "
                  "ORDER BY P.PROCESS_NAME";

    qDebug() << "Executing SQL:" << sql;

    // 5. 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 6. 使用 QStandardItemModel 添加复选框列
    QStandardItemModel *model = new QStandardItemModel(this);
    int colCount = 3;  // 工单、工序、选择（复选框）
    model->setColumnCount(colCount);

    // 7. 设置表头（中文）
    QStringList headers;
    headers << tr("工单") << tr("工序") << tr("选择");
    for (int i = 0; i < headers.size(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    while (query.next()) {
        QString workOrder = query.value(0).toString();
        QString processName = query.value(1).toString();

        // 工单列
        QStandardItem *itemWo = new QStandardItem(workOrder);
        itemWo->setEditable(false);
        // 工序列
        QStandardItem *itemProcess = new QStandardItem(processName);
        itemProcess->setEditable(false);
        // 复选框列
        QStandardItem *itemCheck = new QStandardItem();
        itemCheck->setCheckable(true);
        itemCheck->setCheckState(Qt::Unchecked);
        itemCheck->setEditable(false);
        // 文本对齐（可选）
        itemCheck->setTextAlignment(Qt::AlignCenter);

        model->appendRow(QList<QStandardItem*>() << itemWo << itemProcess << itemCheck);
    }

    // 8. 显示到表格
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    if (model->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("未找到符合条件的关键件记录"));
    }
}
void ClearKeyparts::on_pushButtonSelect_clicked()
{
    UpadteTableRow();
}
void ClearKeyparts::on_pushButtonClear_clicked()
{
    ui->treeWidget->clear();
    clearConditions();
}

void ClearKeyparts::on_pushButtonDelete_clicked()
{
    // 1. 获取选中的行
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->tableView->model());
    if (!model) {
        QMessageBox::warning(this, tr("错误"), tr("表格模型无效"));
        return;
    }

    QList<int> selectedRows;
    for (int row = 0; row < model->rowCount(); ++row) {
        QStandardItem *checkItem = model->item(row, 2);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            selectedRows << row;
        }
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请选择要删除的行"));
        return;
    }

    // 2. 获取条件框中的序列号/重工号
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList reworks = conditions.value("REWOWK");

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

    if (subConditions.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先添加序列号或重工号条件，以免误删"));
        return;
    }

    QString serialSubSql = "SELECT S.SERIAL_NUMBER FROM SAJET.G_SN_STATUS S WHERE " + subConditions.join(" OR ");

    // 3. 数据库连接和事务
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    if (!db.transaction()) {
        QMessageBox::critical(this, tr("错误"), tr("启动事务失败: %1").arg(db.lastError().text()));
        return;
    }

    int totalDeleted = 0;
    for (int row : selectedRows) {
        QString wo = model->item(row, 0)->text();
        QString processName = model->item(row, 1)->text();

        // 获取 PROCESS_ID
        QSqlQuery pidQuery(db);
        pidQuery.prepare("SELECT PROCESS_ID FROM SAJET.SYS_PROCESS WHERE PROCESS_NAME = :process");
        pidQuery.bindValue(":process", processName);
        if (!pidQuery.exec() || !pidQuery.next()) {
            db.rollback();
            QMessageBox::critical(this, tr("错误"), tr("未找到工序: %1").arg(processName));
            return;
        }
        QString processId = pidQuery.value(0).toString();

        // 备份 SQL
        QString backupSql = "INSERT INTO SAJET.G_HT_SN_KEYPARTS "
                            "SELECT K.* FROM SAJET.G_SN_KEYPARTS K "
                            "WHERE K.PROCESS_ID = :pid "
                            "AND K.WORK_ORDER = :wo "
                            "AND K.SERIAL_NUMBER IN (" + serialSubSql + ")";
        qDebug() << "BACKUP SQL:" << backupSql;
        qDebug() << "  PID:" << processId << ", WO:" << wo;

        QSqlQuery backupQuery(db);
        backupQuery.prepare(backupSql);
        backupQuery.bindValue(":pid", processId);
        backupQuery.bindValue(":wo", wo);
        if (!backupQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, tr("错误"), tr("备份失败: %1").arg(backupQuery.lastError().text()));
            return;
        }

        // 删除 SQL
        QString deleteSql = "DELETE FROM SAJET.G_SN_KEYPARTS K "
                            "WHERE K.PROCESS_ID = :pid "
                            "AND K.WORK_ORDER = :wo "
                            "AND K.SERIAL_NUMBER IN (" + serialSubSql + ")";
        qDebug() << "DELETE SQL:" << deleteSql;
        qDebug() << "  PID:" << processId << ", WO:" << wo;

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(deleteSql);
        deleteQuery.bindValue(":pid", processId);
        deleteQuery.bindValue(":wo", wo);
        if (!deleteQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, tr("错误"), tr("删除失败: %1").arg(deleteQuery.lastError().text()));
            return;
        }
        totalDeleted += deleteQuery.numRowsAffected();
    }

    if (!db.commit()) {
        QMessageBox::critical(this, tr("错误"), tr("提交事务失败: %1").arg(db.lastError().text()));
        return;
    }

    QMessageBox::information(this, tr("成功"), tr("已备份并删除 %1 条记录").arg(totalDeleted));
    UpadteTableRow();
}
