#include "materialerp.h"
#include "ui_materialerp.h"
#include "oracle_manager.h"
#include <QSqlQueryModel>
#include <QMessageBox>
#include <QSqlQuery>

MaterialErp::MaterialErp(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MaterialErp)
{
    ui->setupUi(this);
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MaterialErp::onCurrentRowChanged);
}

MaterialErp::~MaterialErp()
{
    delete ui;
}

void MaterialErp::on_lineEditSelect_returnPressed()
{
    // 1. 获取输入并校验
    QString wo = ui->lineEditSelect->text().trimmed();
    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入工单号或者料号"));
        return;
    }

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 3. 构建带参数绑定的查询
    QString sql = "SELECT M.WORK_ORDER, M.ECS_PN, M.ECS_PN_DESC, M.KPARTS_NO, M.CREATE_TIME "
                  "FROM SAJET.ERP_WO_MATERIAL M WHERE M.WORK_ORDER = :wo";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":wo", wo);

    // 4. 执行查询
    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 5. 清理旧模型（如果有）
    QSqlQueryModel *oldModel = qobject_cast<QSqlQueryModel*>(ui->tableView->model());
    if (oldModel) {
        ui->tableView->setModel(nullptr);
        oldModel->deleteLater();
    }

    // 6. 创建新模型
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));

    if (model->lastError().isValid()) {
        QMessageBox::critical(this, tr("错误"), tr("读取数据失败: %1").arg(model->lastError().text()));
        delete model;
        return;
    }

    // 7. 设置自定义表头（使用 tr() 支持多语言）
    QStringList headers;
    headers << tr("工单|料号") << tr("ECS料号") << tr("类型")
            << tr("料件号") << tr("创建时间");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }

    // 8. 设置到表格视图
    ui->tableView->setModel(model);
    ui->tableView->resizeColumnsToContents();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    if (ui->tableView->selectionModel()) {
        connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged,
                this, &MaterialErp::onCurrentRowChanged);
    }
    // 9. 可选：显示查询结果行数
    if (model->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("未找到该物料信息"));
    }
}
void MaterialErp::onCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    // 清空输入框（如果无选中行）
    if (!current.isValid()) {
        ui->lineEditWo->clear();
        ui->lineEditEcsPn->clear();
        ui->lineEditPart->clear();
        ui->comboBoxType->setCurrentIndex(-1);
        return;
    }

    // 获取模型
    QSqlQueryModel *model = qobject_cast<QSqlQueryModel*>(ui->tableView->model());
    if (!model) return;

    int row = current.row();
    // 假设列索引：0-WO, 1-ECS_PN, 2-PART, 3-TYPE (根据实际调整)
    QString wo = model->data(model->index(row, 0)).toString();
    QString ecsPn = model->data(model->index(row, 1)).toString();
    QString part = model->data(model->index(row, 3)).toString();
    QString type = model->data(model->index(row, 2)).toString();

    // 填入控件
    ui->lineEditWo->setText(wo);
    ui->lineEditEcsPn->setText(ecsPn);
    ui->lineEditPart->setText(part);
    int typeIndex = ui->comboBoxType->findText(type);
    ui->comboBoxType->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
}
void MaterialErp::on_pushButtonUpdate_clicked()
{
    // 1. 获取输入数据并校验
    QString wo = ui->lineEditWo->text().trimmed();
    QString ecsPn = ui->lineEditEcsPn->text().trimmed();
    QString part = ui->lineEditPart->text().trimmed();
    QString type = ui->comboBoxType->currentText().trimmed();

    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("工单号不能为空"));
        return;
    }
    if (part.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("料号不能为空"));
        return;
    }

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 3. 构建更新语句（使用参数绑定）
    QString sql = "UPDATE SAJET.ERP_WO_MATERIAL M "
                  "SET M.ECS_PN = :pn, M.ECS_PN_DESC = :type, M.KPARTS_NO = :part "
                  "WHERE M.WORK_ORDER = :wo";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":pn", ecsPn);
    query.bindValue(":type", type);
    query.bindValue(":part", part);
    query.bindValue(":wo", wo);

    // 4. 执行更新
    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("更新失败: %1").arg(query.lastError().text()));
        return;
    }

    // 5. 检查影响行数
    int affected = query.numRowsAffected();
    if (affected == 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到工单 %1，更新失败").arg(wo));
        return;
    }

    // 6. 更新成功，提示并刷新表格
    QMessageBox::information(this, tr("成功"), tr("已更新 %1 条记录").arg(affected));

    QString currentSearch = ui->lineEditSelect->text().trimmed();

    ui->lineEditSelect->setText(wo);
    on_lineEditSelect_returnPressed();
}
void MaterialErp::on_pushButtonAdd_clicked()
{
    // 1. 获取用户输入
    QString woPart = ui->lineEditWo->text().trimmed();
    QString ecsPart = ui->lineEditEcsPn->text().trimmed();
    QString decs = ui->comboBoxType->currentText().trimmed();
    QString kpart = ui->lineEditPart->text().trimmed();

    if (woPart.isEmpty() || decs.isEmpty() || kpart.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"),
                             tr("工单/料号、描述、关键件号均不能为空"));
        return;
    }

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 3. 调用存储过程
    QSqlQuery query(db);
    QString result;
    result.reserve(100);
    query.prepare("BEGIN SAJET.INSERT_WO_MATERIA(:wo_part, :ecs_part, :decs, :kpart, :result); END;");
    query.bindValue(":wo_part", woPart);
    query.bindValue(":ecs_part", ecsPart);
    query.bindValue(":decs", decs);
    query.bindValue(":kpart", kpart);
    query.bindValue(":result", result, QSql::Out);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"),
                              tr("调用存储过程失败: %1").arg(query.lastError().text()));
        return;
    }

    // 5. 获取返回结果
    result = query.boundValue(":result").toString();
    if (result == "OK") {
        QMessageBox::information(this, tr("成功"), tr("物料信息已添加"));

        // 清空输入框（可选）
        ui->lineEditWo->clear();
        ui->lineEditEcsPn->clear();
        ui->lineEditPart->clear();
        ui->comboBoxType->setCurrentIndex(-1);

        // 刷新当前表格（如果查询框有内容，可重新查询）
        QString currentSearch = ui->lineEditSelect->text().trimmed();
        if (!currentSearch.isEmpty()) {
            on_lineEditSelect_returnPressed(); // 重新查询刷新
        }
    } else {
        QMessageBox::warning(this, tr("提示"), tr("添加失败: %1").arg(result));
    }
}


void MaterialErp::on_pushButtonDelete_clicked()
{
    // 1. 获取当前选中的行
    QModelIndex currentIndex = ui->tableView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择要删除的记录"));
        return;
    }

    // 2. 获取模型
    QSqlQueryModel *model = qobject_cast<QSqlQueryModel*>(ui->tableView->model());
    if (!model) {
        QMessageBox::critical(this, tr("错误"), tr("表格模型无效"));
        return;
    }

    int row = currentIndex.row();
    // 列索引：0-WORK_ORDER, 1-ECS_PN, 2-ECS_PN_DESC, 3-KPARTS_NO, 4-CREATE_TIME
    QString wo = model->data(model->index(row, 0)).toString();
    QString ecsPn = model->data(model->index(row, 1)).toString();
    QString ecsDesc = model->data(model->index(row, 2)).toString();
    QString kpart = model->data(model->index(row, 3)).toString();

    if (wo.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("工单号为空，无法删除"));
        return;
    }

    // 3. 确认删除
    int reply = QMessageBox::question(this, tr("确认删除"),
                                      tr("确定要删除工单 %1 的物料记录吗？\nECS料号: %2\n关键件号: %3")
                                          .arg(wo).arg(ecsPn).arg(kpart),
                                      QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    // 4. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 5. 构建删除语句（使用多个字段精确定位）
    QString sql = "DELETE FROM SAJET.ERP_WO_MATERIAL "
                  "WHERE WORK_ORDER = :wo AND ECS_PN = :ecs AND KPARTS_NO = :kpart";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":wo", wo);
    query.bindValue(":ecs", ecsPn);
    query.bindValue(":kpart", kpart);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("删除失败: %1").arg(query.lastError().text()));
        return;
    }

    int affected = query.numRowsAffected();
    if (affected == 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到匹配的记录，可能已被删除"));
        return;
    }

    // 6. 成功提示并刷新表格
    QMessageBox::information(this, tr("成功"), tr("已删除 %1 条记录").arg(affected));

    // 清空输入框
    ui->lineEditWo->clear();
    ui->lineEditEcsPn->clear();
    ui->lineEditPart->clear();
    ui->comboBoxType->setCurrentIndex(-1);
}

