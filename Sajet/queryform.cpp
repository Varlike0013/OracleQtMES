#include "queryform.h"
#include "ui_queryform.h"
#include "oracle_manager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QSqlQueryModel>

QueryForm::QueryForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::QueryForm)
{
    ui->setupUi(this);
    loadSQL();
}
QueryForm::~QueryForm()
{
    delete ui;
}
QString QueryForm::configFilePath()
{
    return QCoreApplication::applicationDirPath() + "/config/queries.json";
}
void QueryForm::on_pushButtonAdd_clicked()
{
    // 1. 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("添加 SQL 语句"));

    QFormLayout *form = new QFormLayout(&dialog);

    // 名称输入
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    form->addRow(tr("名称:"), nameEdit);

    // 类型选择
    QComboBox *typeCombo = new QComboBox(&dialog);
    typeCombo->addItems(QStringList() << tr("查询") << tr("修改") << tr("删除") << tr("增加") << tr("其他"));
    form->addRow(tr("类型:"), typeCombo);

    // 描述输入
    QLineEdit *descEdit = new QLineEdit(&dialog);
    form->addRow(tr("描述:"), descEdit);

    // SQL 语句输入
    QPlainTextEdit *sqlEdit = new QPlainTextEdit(&dialog);
    sqlEdit->setPlaceholderText(tr("请输入 SQL 语句..."));
    form->addRow(tr("SQL 语句:"), sqlEdit);

    // 确定/取消按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 2. 执行对话框
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 3. 获取输入
    QString name = nameEdit->text().trimmed();
    QString type = typeCombo->currentText();
    int typeId = typeCombo->currentIndex();
    QString description = descEdit->text().trimmed();
    QString sql = sqlEdit->toPlainText().trimmed();

    // 4. 校验
    if (name.isEmpty() || sql.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("名称和 SQL 语句不能为空"));
        return;
    }

    // 5. 保存到 JSON
    QString FILE_PATH = configFilePath();
    QFile file(FILE_PATH);
    QJsonArray queries;

    // 确保 config 目录存在
    QDir dir = QFileInfo(FILE_PATH).absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, tr("错误"), tr("无法创建配置目录: %1").arg(dir.path()));
            return;
        }
    }

    // 读取现有数据
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            queries = doc.array();
        }
    }

    // 添加新记录
    QJsonObject newQuery;
    newQuery["name"] = name;
    newQuery["type"] = type;
    newQuery["typeId"] = typeId;
    newQuery["description"] = description;
    newQuery["sql"] = sql;
    queries.append(newQuery);

    // 写入文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法保存文件: %1").arg(FILE_PATH));
        return;
    }
    QJsonDocument saveDoc(queries);
    file.write(saveDoc.toJson(QJsonDocument::Indented));
    file.close();
    loadSQL();
    QMessageBox::information(this, tr("成功"), tr("SQL 语句已保存"));
}
void QueryForm::loadSQL()
{
    // 1. 清空树
    ui->treeWidget->clear();
    m_selectedItem = nullptr;

    // 2. 读取 JSON 文件
    QString filePath = configFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取配置文件: %1").arg(filePath));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Invalid JSON format in queries.json";
        return;
    }

    QJsonArray queries = doc.array();

    // 3. 遍历数组，添加到树中
    for (const QJsonValue &value : queries) {
        if (!value.isObject()) continue;
        QJsonObject obj = value.toObject();

        QString name = obj["name"].toString();
        QString type = obj["type"].toString();

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, name);
        item->setText(1, type);

        // 将完整 SQL 存储到 UserRole 中，以便点击时显示或执行
        item->setData(0, Qt::UserRole, name);
        item->setData(0, Qt::UserRole+1, type);
        item->setData(0, Qt::UserRole+2, obj["sql"].toString());
        item->setData(0, Qt::UserRole+3, obj["description"].toString());
        item->setData(0, Qt::UserRole+4, obj["typeId"].toInt());
    }

    // 展开所有节点（虽然只有一层）
    ui->treeWidget->expandAll();
}
void QueryForm::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        m_selectedItem = nullptr;
        // 清空显示
        ui->labelName->clear();
        ui->plainTextEditDesc->clear();
        ui->plainTextEditSql->clear();
        return;
    }

    m_selectedItem = item; // 保存当前选中的项

    QString name = item->data(0, Qt::UserRole).toString();
    QString sql = item->data(0, Qt::UserRole+2).toString();
    QString desc = item->data(0, Qt::UserRole+3).toString();
    ui->labelName->setText(name);
    ui->plainTextEditDesc->setPlainText(desc);
    ui->plainTextEditSql->setPlainText(sql);
}
void QueryForm::on_pushButtoBuild_clicked()
{
    // 1. 获取 SQL 语句
    QString sql = ui->plainTextEditSql->toPlainText().trimmed();
    if (sql.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("SQL 语句为空，请先加载或输入"));
        return;
    }

    // 2. 清除旧的参数输入框
    QLayout *oldLayout = ui->groupBox->layout();
    if (oldLayout) {
        delete oldLayout;
    }
    m_paramEdits.clear();
    m_paramNames.clear();

    // 3. 解析参数（形如 :name）
    QRegularExpression re(":([a-zA-Z_][a-zA-Z0-9_]*)");
    QRegularExpressionMatchIterator it = re.globalMatch(sql);
    QStringList paramNames;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString param = match.captured(1);
        if (!paramNames.contains(param)) {
            paramNames.append(param);
        }
    }

    if (paramNames.isEmpty()) {
        // 无参数，显示提示
        QLabel *noParamLabel = new QLabel(tr("此 SQL 无需参数，可直接执行"));
        QVBoxLayout *layout = new QVBoxLayout(ui->groupBox);
        layout->addWidget(noParamLabel);
        ui->groupBox->setLayout(layout);
        return;
    }

    // 4. 创建参数输入框（使用 QFormLayout）
    QFormLayout *formLayout = new QFormLayout(ui->groupBox);
    for (const QString &param : paramNames) {
        QLabel *label = new QLabel(param, ui->groupBox);
        QLineEdit *edit = new QLineEdit(ui->groupBox);
        edit->setObjectName(param);
        formLayout->addRow(label, edit);
        m_paramEdits[param] = edit;
        m_paramNames.append(param);
    }
    ui->groupBox->setLayout(formLayout);
}

void QueryForm::on_pushButtonExecute_clicked()
{
    if (!m_selectedItem) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一条 SQL 语句"));
        return;
    }

    // 获取 SQL 和描述
    QString sql = m_selectedItem->data(0, Qt::UserRole + 2).toString();
    int typeId = m_selectedItem->data(0, Qt::UserRole + 4).toInt();
    if (sql.endsWith(';')) {
        sql.chop(1);  // 去掉末尾分号
    }

    if (sql.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("SQL 语句为空"));
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 收集参数值（如果存在）
    QMap<QString, QString> paramValues;
    if (!m_paramNames.isEmpty()) {
        for (const QString &param : m_paramNames) {
            QLineEdit *edit = m_paramEdits[param];
            if (edit) {
                paramValues[param] = edit->text().trimmed();
            } else {
                paramValues[param] = "";
            }
        }
    }

    // 准备执行
    QSqlQuery query(db);
    query.prepare(sql);
    // 绑定参数
    for (auto it = paramValues.begin(); it != paramValues.end(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }

    switch (typeId) {
    case 0: // SELECT
    {
        if (!query.exec()) {
            QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
            return;
        }

        // 使用 QSqlQueryModel 显示结果
        QSqlQueryModel *model = new QSqlQueryModel(this);
        model->setQuery(std::move(query));
        if (model->lastError().isValid()) {
            QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
            delete model;
            return;
        }

        // 设置到 tableView（假设 UI 中有 tableView）
        ui->tableView->setModel(model);
        ui->tableView->resizeColumnsToContents();
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

        if (model->rowCount() == 0) {
            QMessageBox::information(this, tr("提示"), tr("查询结果为空"));
        } else {
            QMessageBox::information(this, tr("成功"), tr("查询完成，共 %1 行").arg(model->rowCount()));
        }
        break;
    }
    case 1: // UPDATE
    case 2: // DELETE
    case 3: // INSERT
    {
        if (!query.exec()) {
            QMessageBox::critical(this, tr("错误"), tr("执行失败: %1").arg(query.lastError().text()));
            return;
        }
        int affected = query.numRowsAffected();
        QMessageBox::information(this, tr("成功"), tr("操作成功，影响 %1 行").arg(affected));
        break;
    }
    default:
        QMessageBox::warning(this, tr("错误"), tr("未知的操作类型"));
        break;
    }
}
void QueryForm::on_pushButtonExport_clicked()
{
    QString result = OracleManager::exportTableViewToCsv(ui->tableView,this);
    if (result.isEmpty()) {
        QMessageBox::information(this, tr("成功"), tr("导出成功"));
    } else {
        QMessageBox::critical(this, tr("错误"), tr("导出失败: %1").arg(result));
    }
}
void QueryForm::on_pushButtonUpdate_clicked()
{
    if (!m_selectedItem) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一条 SQL 语句"));
        return;
    }

    // 获取 SQL 和描述
    QString name = m_selectedItem->data(0, Qt::UserRole).toString();
    QString sql = ui->plainTextEditSql->toPlainText().trimmed();
    QString desc = ui->plainTextEditDesc->toPlainText().trimmed();

    if (sql.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("SQL 语句不能为空"));
        return;
    }
    if (desc.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("描述不能为空"));
        return;
    }

    // 配置文件路径
    QString filePath = QueryForm::configFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        QMessageBox::warning(this, tr("错误"), tr("配置文件不存在，请先添加"));
        return;
    }

    // 读取 JSON
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法读取配置文件: %1").arg(filePath));
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::critical(this, tr("错误"), tr("配置文件格式错误"));
        return;
    }

    QJsonArray queries = doc.array();
    bool found = false;
    for (int i = 0; i < queries.size(); ++i) {
        QJsonObject obj = queries[i].toObject();
        if (obj["name"].toString() == name) {
            obj["sql"] = sql;
            obj["description"] = desc;
            queries[i] = obj;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, tr("错误"), tr("未找到名为 '%1' 的记录").arg(name));
        return;
    }

    // 写回文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法写入配置文件: %1").arg(filePath));
        return;
    }
    QJsonDocument saveDoc(queries);
    file.write(saveDoc.toJson(QJsonDocument::Indented));
    file.close();

    // 更新树节点存储的数据（SQL 和描述）
    m_selectedItem->setData(0, Qt::UserRole + 2, sql);
    m_selectedItem->setData(0, Qt::UserRole + 3, desc);

    QMessageBox::information(this, tr("成功"), tr("SQL 语句已更新"));
}
void QueryForm::on_pushButtonDelete_clicked()
{
    if (!m_selectedItem) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一条 SQL 语句"));
        return;
    }

    QString name = m_selectedItem->text(0);
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("无法获取记录名称"));
        return;
    }

    // 确认删除
    int reply = QMessageBox::question(this, tr("确认删除"),
                                      tr("确定要删除 SQL 语句 \"%1\" 吗？").arg(name),
                                      QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    // 配置文件路径
    QString filePath = QueryForm::configFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        QMessageBox::warning(this, tr("错误"), tr("配置文件不存在"));
        return;
    }

    // 读取 JSON
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法读取配置文件: %1").arg(filePath));
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::critical(this, tr("错误"), tr("配置文件格式错误"));
        return;
    }

    QJsonArray queries = doc.array();
    bool found = false;
    for (int i = 0; i < queries.size(); ++i) {
        QJsonObject obj = queries[i].toObject();
        if (obj["name"].toString() == name) {
            queries.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(this, tr("错误"), tr("未找到名为 '%1' 的记录").arg(name));
        return;
    }

    // 写回文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法写入配置文件: %1").arg(filePath));
        return;
    }
    QJsonDocument saveDoc(queries);
    file.write(saveDoc.toJson(QJsonDocument::Indented));
    file.close();

    // 重新加载树（清空选中项）
    m_selectedItem = nullptr;
    loadSQL();

    // 清空显示区域
    ui->labelName->clear();
    ui->plainTextEditDesc->clear();
    ui->plainTextEditSql->clear();

    QMessageBox::information(this, tr("成功"), tr("已删除 SQL 语句 \"%1\"").arg(name));
}

void QueryForm::on_lineEditSelect_returnPressed()
{
    QString text = ui->lineEditSelect->text();
    QString searchText = text.trimmed();
    bool hasSearch = !searchText.isEmpty();

    // 遍历顶层分类节点（"表"、"存储过程"）
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = ui->treeWidget->topLevelItem(i);
        if (!category) continue;

        // 递归设置可见性
        OracleManager::setItemVisibility(category, searchText, hasSearch);

        // 展开/折叠分类节点
        if (hasSearch) {    // 如果有可见子节点则展开，否则折叠
            category->setExpanded(OracleManager::hasVisibleChild(category));
        } else {    // 无搜索时全部折叠
            category->setExpanded(false);
        }
    }
}
