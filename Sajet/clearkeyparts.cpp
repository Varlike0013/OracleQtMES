#include "clearkeyparts.h"
#include "ui_clearkeyparts.h"
#include "managersajet.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlQueryModel>


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
    QString sql = "SELECT K.WORK_ORDER, K.SERIAL_NUMBER, P.PROCESS_NAME, PA.PART_NO, PA.PART_TYPE, "
                  "K.ITEM_PART_SN, K.ITEM_GROUP, K.VERSION, E.EMP_NAME, K.UPDATE_TIME "
                  "FROM SAJET.G_SN_KEYPARTS K "
                  "LEFT JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = K.PROCESS_ID "
                  "LEFT JOIN SAJET.SYS_PART PA ON PA.PART_ID = K.ITEM_PART_ID "
                  "LEFT JOIN SAJET.SYS_EMP E ON E.EMP_ID = K.UPDATE_USERID "
                  "WHERE K.SERIAL_NUMBER IN (" + subSql + ")";

    qDebug() << "Executing SQL:" << sql;

    // 5. 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 6. 创建模型
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }

    // 7. 设置表头（10列）
    QStringList headers;
    headers << tr("工单") << tr("序列号") << tr("工序") << tr("料号")
            << tr("料件类型") << tr("料件SN") << tr("分组") << tr("版本")
            << tr("上传人员") << tr("上传时间");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 8. 显示到表格
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 9. 行数提示（可选）
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

