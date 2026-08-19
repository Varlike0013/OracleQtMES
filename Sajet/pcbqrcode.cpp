#include "pcbqrcode.h"
#include "ui_pcbqrcode.h"
#include "managersajet.h"
#include <QMessageBox>
#include <qlabel.h>
#include <qsqlquery.h>
#include <QSqlQueryModel>
#include <QFormLayout>

QMap<QString, QStringList> PcbQrcode::m_conditions;

PcbQrcode::PcbQrcode(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PcbQrcode)
{
    ui->setupUi(this);
}

PcbQrcode::~PcbQrcode()
{
    delete ui;
}
void PcbQrcode::addCondition(const QString &key, const QString &value)
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

bool PcbQrcode::removeCondition(const QString &key, const QString &value)
{
    if (!m_conditions.contains(key)) return false;
    bool removed = m_conditions[key].removeOne(value);
    if (m_conditions[key].isEmpty()) {
        m_conditions.remove(key);
    }
    return removed;
}

void PcbQrcode::clearConditions()
{
    m_conditions.clear();
}

QStringList PcbQrcode::getConditionValues(const QString &key)
{
    return m_conditions.value(key, QStringList());
}

QMap<QString, QStringList> PcbQrcode::getAllConditions()
{
    return m_conditions;
}

bool PcbQrcode::conditionExists(const QString &key, const QString &value)
{
    return m_conditions.contains(key) && m_conditions[key].contains(value);
}
void PcbQrcode::on_lineEdit_returnPressed()
{
    int index = ui->comboBox->currentIndex();
    QString text = ui->lineEdit->text().trimmed();

    if (text.isEmpty()) {
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
        if (ManagerSajet::is_SERIAL_NUMBER(text)) {
            key = "SN";
            valid = true;
        } else {
            QMessageBox::warning(this, tr("输入错误"), tr("序列号不存在"));
        }
    } else if (index == 1) { // 二维码
        key = "QRCode";
        valid = true;
    } else {
        QMessageBox::warning(this, tr("未知错误"), tr("未知错误"));
    }

    if (valid) {
        // 1. 添加到树控件
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, key);
        item->setText(1, text);
        ui->treeWidget->addTopLevelItem(item);
        ui->treeWidget->expandAll();

        // 2. 同步添加到全局字典
        addCondition(key, text);

        // 3. 清空输入框
        ui->lineEdit->clear();
        UpadteTableRow();
    }
}
void PcbQrcode::UpadteTableRow()
{
    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList qrcodes = conditions.value("QRCode");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && qrcodes.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加序列号或其他条件"));
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
        subConditions << "E.STRSMTSN IN (" + quoteAndJoin(serials) + ")";
    }
    if (!qrcodes.isEmpty()) {
        subConditions << "E.PCB_QRCODE IN (" + quoteAndJoin(qrcodes) + ")";
    }

    if (subConditions.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请添加查询条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 构建完整 SQL（6个字段）
    QString sql = "SELECT E.ECS_PART_NO, E.PCB_CUST_PN, E.PCB_SN, E.STRSMTSN, E.PCB_QRCODE, E.CREATE_TIME "
                  "FROM SAJET.ECS_PPID_PCB_CODE E "
                  "WHERE " + subConditions.join(" OR ");

    qDebug() << "Executing SQL:" << sql;

    // 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 创建模型
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }

    // 设置表头（6列）
    QStringList headers;
    headers << tr("ECS零件号") << tr("PCB客户PN") << tr("PCB SN")
            << tr("STR SMT SN") << tr("PCB二维码") << tr("创建时间");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 显示到表格
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}
void PcbQrcode::on_pushButtonSelect_clicked()
{
    UpadteTableRow();
}
void PcbQrcode::on_pushButtonClear_clicked()
{
    ui->treeWidget->clear();
    clearConditions();
}

void PcbQrcode::on_pushButtonDelete_clicked()
{
    // 1. 获取所有条件（键：序列号 / 重工号）
    QMap<QString, QStringList> conditions = getAllConditions();
    QStringList serials = conditions.value("SN");
    QStringList qrcodes = conditions.value("QRCode");

    // 2. 若两个列表都为空，提示并清空表格
    if (serials.isEmpty() && qrcodes.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加序列号或其他条件"));
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
        subConditions << "E.STRSMTSN IN (" + quoteAndJoin(serials) + ")";
    }
    if (!qrcodes.isEmpty()) {
        subConditions << "E.PCB_QRCODE IN (" + quoteAndJoin(qrcodes) + ")";
    }

    if (subConditions.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请添加查询条件"));
        ui->tableView->setModel(nullptr);
        return;
    }

    // 构建完整 SQL（6个字段）
    QString sql = "DELETE FROM SAJET.ECS_PPID_PCB_CODE E "
                  "WHERE " + subConditions.join(" OR ");

    if (!db.transaction()) {
        QMessageBox::critical(this, tr("错误"), tr("启动事务失败: %1").arg(db.lastError().text()));
        return;
    }

    QSqlQuery query(db);
    if (!query.exec(sql)) {
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
    UpadteTableRow();
}
void PcbQrcode::on_pushButtonAdd_clicked()
{
    // 1. 创建输入对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("新增PCB记录"));
    QFormLayout *formLayout = new QFormLayout(&dialog);

    QLineEdit *editEcsPartNo = new QLineEdit(&dialog);
    QLineEdit *editPcbCustPn = new QLineEdit(&dialog);
    QLineEdit *editPcbSn = new QLineEdit(&dialog);
    QLineEdit *editStrSmtsn = new QLineEdit(&dialog);
    QLineEdit *editPcbQrcode = new QLineEdit(&dialog);

    // 用于显示创建时间的只读标签
    QLabel *labelCreateTime = new QLabel(tr("等待输入序列号..."), &dialog);
    labelCreateTime->setStyleSheet("QLabel { color: blue; font-weight: bold; }");

    formLayout->addRow(tr("ECS零件号:"), editEcsPartNo);
    formLayout->addRow(tr("PCB客户PN:"), editPcbCustPn);
    formLayout->addRow(tr("PCB SN:"), editPcbSn);
    formLayout->addRow(tr("STR SMTSN:"), editStrSmtsn);
    formLayout->addRow(tr("PCB二维码:"), editPcbQrcode);
    formLayout->addRow(tr("创建时间:"), labelCreateTime);

    QPushButton *btnOk = new QPushButton(tr("确定"), &dialog);
    QPushButton *btnCancel = new QPushButton(tr("取消"), &dialog);
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    formLayout->addRow(btnLayout);

    // 连接确定/取消按钮
    connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    // 2. 当 STR SMTSN 输入变化时，查询创建时间
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(&dialog, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 查询函数（接受序列号）
    auto queryCreateTime = [&](const QString &sn) {
        if (sn.isEmpty()) {
            labelCreateTime->setText(tr("等待输入序列号..."));
            labelCreateTime->setStyleSheet("QLabel { color: blue; }");
            return;
        }

        QSqlQuery query(db);
        query.prepare("SELECT MAX(T.OUT_PROCESS_TIME) FROM SAJET.G_SN_TRAVEL T "
                      "WHERE T.SERIAL_NUMBER = :sn AND T.PROCESS_ID = 200204");
        query.bindValue(":sn", sn);

        if (!query.exec()) {
            labelCreateTime->setText(tr("查询失败"));
            labelCreateTime->setStyleSheet("QLabel { color: red; }");
            return;
        }

        if (query.next() && !query.value(0).isNull()) {
            QDateTime dt = query.value(0).toDateTime();
            labelCreateTime->setText(dt.toString("yyyy-MM-dd hh:mm:ss"));
            labelCreateTime->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            QDateTime now = QDateTime::currentDateTime();
            labelCreateTime->setText(now.toString("yyyy-MM-dd hh:mm:ss"));
            labelCreateTime->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    };

    // 当 STRSMTSN 输入内容变化时触发查询
    connect(editStrSmtsn, &QLineEdit::textChanged, [&]() {
        queryCreateTime(editStrSmtsn->text());
    });

    // 3. 显示对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted) {
        return; // 用户取消
    }

    // 4. 获取用户输入
    QString ecsPartNo = editEcsPartNo->text().trimmed();
    QString pcbCustPn = editPcbCustPn->text().trimmed();
    QString pcbSn = editPcbSn->text().trimmed();
    QString strSmtsn = editStrSmtsn->text().trimmed();
    QString pcbQrcode = editPcbQrcode->text().trimmed();

    // 检查必填字段（至少 STR SMTSN 不能为空）
    if (strSmtsn.isEmpty()&&pcbQrcode.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("SN和二维码不能为空"));
        return;
    }

    // 5. 获取当前显示的创建时间（确保有效）
    QString timeStr = labelCreateTime->text();
    if (timeStr.contains(tr("未找到")) || timeStr.contains(tr("等待输入"))) {
        QMessageBox::critical(this, tr("错误"), tr("请先输入有效的 STR SMTSN 以获取创建时间"));
        return;
    }

    // 6. 解析时间字符串（或直接重新查询一次，更可靠）
    QDateTime createTime;
    // 由于 label 中可能包含颜色样式等，最好重新查询确保准确性
    QSqlQuery query(db);
    query.prepare("SELECT MAX(T.OUT_PROCESS_TIME) FROM SAJET.G_SN_TRAVEL T "
                  "WHERE T.SERIAL_NUMBER = :sn AND T.PROCESS_ID = 200204");
    query.bindValue(":sn", strSmtsn);
    if (!query.exec() || !query.next() || query.value(0).isNull()) {
        QMessageBox::critical(this, tr("错误"), tr("创建时间无效，请检查序列号"));
        return;
    }
    createTime = query.value(0).toDateTime();

    QString confirmMsg = tr("确认插入以下记录？\n\n")
                         + tr("ECS零件号: %1\n").arg(ecsPartNo)
                         + tr("PCB客户PN: %2\n").arg(pcbCustPn)
                         + tr("PCB SN: %3\n").arg(pcbSn)
                         + tr("STR SMTSN: %4\n").arg(strSmtsn)
                         + tr("PCB二维码: %5\n").arg(pcbQrcode)
                         + tr("创建时间: %6").arg(createTime.toString("yyyy-MM-dd hh:mm:ss"));

    int reply = QMessageBox::question(this, tr("确认插入"), confirmMsg,
                                      QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return; // 用户取消
    }

    // 7. 插入数据
    QString insertSql = "INSERT INTO SAJET.ECS_PPID_PCB_CODE "
                        "(ECS_PART_NO, PCB_CUST_PN, PCB_SN, STRSMTSN, PCB_QRCODE, CREATE_TIME) "
                        "VALUES (:ecs, :custpn, :pcbsn, :strsmtsn, :qrcode, :create_time)";
    QSqlQuery insertQuery(db);
    insertQuery.prepare(insertSql);
    insertQuery.bindValue(":ecs", ecsPartNo);
    insertQuery.bindValue(":custpn", pcbCustPn);
    insertQuery.bindValue(":pcbsn", pcbSn);
    insertQuery.bindValue(":strsmtsn", strSmtsn);
    insertQuery.bindValue(":qrcode", pcbQrcode);
    insertQuery.bindValue(":create_time", createTime);

    if (!insertQuery.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("插入数据失败: %1").arg(insertQuery.lastError().text()));
        return;
    }

    QMessageBox::information(this, tr("成功"), tr("记录已添加"));
}
void PcbQrcode::on_tableView_clicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QAbstractItemModel *model = ui->tableView->model();
    if (!model) return;

    int row = index.row();
    // 列顺序：0-ECS零件号, 1-PCB客户PN, 2-PCB SN, 3-STR SMTSN, 4-PCB二维码, 5-创建时间
    QString ecsPartNo = model->data(model->index(row, 0)).toString();
    QString pcbCustPn = model->data(model->index(row, 1)).toString();
    QString pcbSn = model->data(model->index(row, 2)).toString();
    QString strSmtsn = model->data(model->index(row, 3)).toString();
    QString pcbQrcode = model->data(model->index(row, 4)).toString();
    // 不读取创建时间（列5）

    // 填入对应的 QLineEdit（根据您的控件名称调整）
    ui->lineEditEcsPartNo->setText(ecsPartNo);
    ui->lineEditPcbCustPn->setText(pcbCustPn);
    ui->lineEditPcbSn->setText(pcbSn);
    ui->lineEditStrSmtsn->setText(strSmtsn);
    ui->lineEditPcbQrcode->setText(pcbQrcode);
}

void PcbQrcode::on_pushButtonUpdate_clicked()
{

    QString ecsPartNo = ui->lineEditEcsPartNo->text().trimmed();
    QString pcbCustPn = ui->lineEditPcbCustPn->text().trimmed();
    QString pcbSn = ui->lineEditPcbSn->text().trimmed();
    QString strSmtsn = ui->lineEditStrSmtsn->text().trimmed();
    QString pcbQrcode = ui->lineEditPcbQrcode->text().trimmed();

    if (strSmtsn.isEmpty() || pcbQrcode.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("STR SMTSN 和 PCB 二维码不能为空"));
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 5. 构建更新语句（使用主键条件，保留原创建时间）
    QString sql = "UPDATE SAJET.ECS_PPID_PCB_CODE "
                  "SET ECS_PART_NO = :ecs, PCB_CUST_PN = :cust, PCB_SN = :sn, PCB_QRCODE = :qrcode "
                  "WHERE STRSMTSN = :str";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":ecs", ecsPartNo);
    query.bindValue(":cust", pcbCustPn);
    query.bindValue(":sn", pcbSn);
    query.bindValue(":str", strSmtsn);
    query.bindValue(":qrcode", pcbQrcode);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("更新失败: %1").arg(query.lastError().text()));
        return;
    }

    int affected = query.numRowsAffected();
    if (affected == 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到匹配的记录，可能已被修改或删除"));
        return;
    }
    QMessageBox::information(this, tr("成功"), tr("已更新 %1 条记录").arg(affected));

    UpadteTableRow(); // 更新表格
}

