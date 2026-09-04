#include "checkemp.h"
#include "ui_checkemp.h"
#include "oracle_manager.h"
#include "managersajet.h"
#include <QSqlQueryModel>
#include <QMessageBox>
#include <QStandardItemModel>

CheckEMP::CheckEMP(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CheckEMP)
{
    ui->setupUi(this);
    loadRole();
    ManagerSajet::loadDept(ui->comboBoxDept);
}

CheckEMP::~CheckEMP()
{
    delete ui;
}

void CheckEMP::on_lineEditInput_returnPressed()
{
    // 获取输入内容
    QString input = ui->lineEditInput->text().trimmed();
    if (input.isEmpty()) {
        ui->tableViewEmp->setModel(nullptr);
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when loading processes.";
        return;
    }

    int index = ui->comboBoxType->currentIndex();
    QString whereClause;
    // 根据下拉框构建不同的查询条件
    switch (index) {
    case 0:  // 按工号精确查询
        whereClause = "E.EMP_NO = :empNo";
        break;
    case 1:  // 按姓名模糊查询（支持部分匹配）
        whereClause = "E.EMP_NAME LIKE :empName";
        input = "%" + input + "%";
        break;
    default:
        whereClause = "1=1";
        break;
    }

    // 准备 SQL 语句
    QString sql = "SELECT E.EMP_NO, E.EMP_NAME, E.EMAIL, E.UPDATE_TIME, D.DEPT_NAME "
                  "FROM SAJET.SYS_EMP E "
                  "LEFT JOIN SAJET.SYS_DEPT D ON D.DEPT_ID = E.DEPT_ID "
                  "WHERE " + whereClause + " "
                                  "ORDER BY E.UPDATE_TIME DESC";
    qDebug()<<sql;
    QSqlQuery query(db);
    query.prepare(sql);

    // 绑定参数
    if (index == 0) {
        query.bindValue(":empNo", input);
    } else if (index == 1) {
        query.bindValue(":empName", input);
    }

    // 执行查询
    if (!query.exec()) {
        QMessageBox::critical(this, "查询失败", query.lastError().text());
        return;
    }

    // 设置模型（自动释放旧模型）
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(query);
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, "模型错误", model->lastError().text());
        delete model;
        return;
    }
    // 设置表头
    QStringList headers;
    headers << tr("工号") << tr("姓名") << tr("邮箱")
            << tr("更新时间") << tr("部门");
    for (int i = 0; i < headers.size() && i < model->columnCount(); ++i) {
        model->setHeaderData(i, Qt::Horizontal, headers[i]);
    }
    ui->tableViewEmp->setModel(model);
    ui->tableViewEmp->resizeColumnsToContents();
}
void CheckEMP::loadRole()
{
    // 1. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when loading roles.";
        return;
    }

    // 2. 执行查询
    QSqlQuery query(db);
    QString sql = "SELECT R.ROLE_ID,R.ROLE_NAME, R.ROLE_DESC "
                  "FROM SAJET.SYS_ROLE R "
                  "WHERE R.ENABLED = 'Y' "
                  "ORDER BY R.ROLE_NAME";
    if (!query.exec(sql)) {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    // 3. 创建标准模型（三列：角色名，描述，复选框）
    QStandardItemModel *model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels(QStringList() << tr("角色名") << tr("描述") << ("规则选中状态"));

    // 4. 填充数据
    while (query.next()) {
        QString roleId = query.value(0).toString();
        QString roleName = query.value(1).toString();
        QString roleDesc = query.value(2).toString();

        QList<QStandardItem*> rowItems;
        QStandardItem *nameItem = new QStandardItem(roleName);
        nameItem->setEditable(false);   // 禁止编辑
        QStandardItem *descItem = new QStandardItem(roleDesc);
        descItem->setEditable(false);
        QStandardItem *checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(Qt::Unchecked);   // 默认不选中
        checkItem->setEditable(false);
        checkItem->setData(roleId, Qt::UserRole);
        rowItems << nameItem << descItem << checkItem;
        model->appendRow(rowItems);
    }

    // 5. 设置模型到表格视图
    ui->tableViewRole->setModel(model);
    ui->tableViewRole->resizeColumnsToContents();
    ui->tableViewRole->horizontalHeader()->setStretchLastSection(true);
    // 可选：使复选框列宽度固定
    ui->tableViewRole->setColumnWidth(2, 30);
}

void CheckEMP::on_tableViewEmp_clicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QAbstractItemModel *empModel = ui->tableViewEmp->model();
    if (!empModel) return;

    QString empNo = empModel->data(empModel->index(index.row(), 0)).toString();
    if (empNo.isEmpty()) {
        qWarning() << "员工工号为空，请检查列索引。";
        return;
    }
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "数据库连接无效";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT R.ROLE_ID FROM SAJET.SYS_ROLE_EMP R "
                  "WHERE R.EMP_ID = (SELECT EMP_ID FROM SAJET.SYS_EMP WHERE EMP_NO = :eno)");
    query.bindValue(":eno", empNo);
    if (!query.exec()) {
        qWarning() << "查询角色失败:" << query.lastError().text();
        return;
    }

    m_roleIds.clear();
    m_empName = empNo;
    while (query.next()) {
        m_roleIds.insert(query.value(0).toInt());
    }

    QAbstractItemModel *roleModel = ui->tableViewRole->model();
    if (!roleModel) {
        qWarning() << "角色表格模型为空，请先调用 loadRole()";
        return;
    }

    int checkColumn = 2;
    for (int row = 0; row < roleModel->rowCount(); ++row) {
        QModelIndex checkIndex = roleModel->index(row, checkColumn);
        if (!checkIndex.isValid()) continue;

        int roleId = roleModel->data(checkIndex, Qt::UserRole).toInt();

        bool hasRole = m_roleIds.contains(roleId);
        Qt::CheckState state = hasRole ? Qt::Checked : Qt::Unchecked;

        // 更新复选框状态（需将模型转为 QStandardItemModel）
        QStandardItemModel *stdModel = qobject_cast<QStandardItemModel*>(roleModel);
        if (stdModel) {
            QStandardItem *item = stdModel->item(row, checkColumn);
            if (item) {
                item->setCheckState(state);
            }
        } else {
            roleModel->setData(checkIndex, state, Qt::CheckStateRole);
        }
    }
}
void CheckEMP::on_pushButtonSave_clicked()
{
    // 1. 获取当前选中的角色ID集合
    QSet<int> currentRoleIds;
    QAbstractItemModel *roleModel = ui->tableViewRole->model();
    if (!roleModel) {
        QMessageBox::warning(this, tr("错误"), tr("角色列表为空，请先加载"));
        return;
    }
    for (int row = 0; row < roleModel->rowCount(); ++row) {
        QModelIndex checkIdx = roleModel->index(row, 2); // 复选框列索引
        if (!checkIdx.isValid()) continue;
        Qt::CheckState state = static_cast<Qt::CheckState>(roleModel->data(checkIdx, Qt::CheckStateRole).toInt());
        if (state == Qt::Checked) {
            int roleId = roleModel->data(checkIdx, Qt::UserRole).toInt();
            currentRoleIds.insert(roleId);
        }
    }

    // 2. 比较当前选中的和原有的，找出需要添加和删除的
    QSet<int> toAdd = currentRoleIds - m_roleIds;      // 新增
    QSet<int> toRemove = m_roleIds - currentRoleIds;   // 移除

    if (toAdd.isEmpty() && toRemove.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有发生变更"));
        return;
    }

    // 3. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 4. 获取当前员工的EMP_ID
    QString empId;
    QSqlQuery queryEmp(db);
    queryEmp.prepare("SELECT EMP_ID FROM SAJET.SYS_EMP WHERE EMP_NO = :empNo");
    queryEmp.bindValue(":empNo", m_empName);
    if (!queryEmp.exec() || !queryEmp.next()) {
        QMessageBox::critical(this, tr("错误"), tr("未找到员工信息"));
        return;
    }
    empId = queryEmp.value(0).toString();

    // 5. 执行数据库更新（事务）
    db.transaction();
    bool ok = true;

    QSqlQuery query(db);

    // 删除移除的角色
    if (!toRemove.isEmpty()) {
        query.prepare("DELETE FROM SAJET.SYS_ROLE_EMP WHERE EMP_ID = :empId AND ROLE_ID = :roleId");
        for (int roleId : toRemove) {
            query.bindValue(":empId", empId);
            query.bindValue(":roleId", roleId);
            if (!query.exec()) {
                // 检查错误类型，如果是 ORA-24333（零行影响）则忽略
                if (query.lastError().nativeErrorCode() != "24333") {
                    qWarning() << "删除角色失败:" << query.lastError().text();
                    ok = false;
                    break;
                }
            }
        }
    }

    // 添加新增的角色
    if (ok && !toAdd.isEmpty()) {
        query.prepare("INSERT INTO SAJET.SYS_ROLE_EMP (EMP_ID, ROLE_ID) VALUES (?, ?)");
        for (int roleId : toAdd) {
            query.addBindValue(empId);
            query.addBindValue(roleId);
            if (!query.exec()) {
                qDebug() << "Insert role failed:" << query.lastError().text();
                ok = false;
                break;
            }
        }
    }

    if (ok) {
        db.commit();
        // 更新内存中的角色集合
        m_roleIds = currentRoleIds;
        QMessageBox::information(this, tr("成功"), tr("角色权限已更新"));
    } else {
        db.rollback();
        QMessageBox::critical(this, tr("错误"), tr("更新角色失败"));
    }
}


void CheckEMP::on_pushButtonClear_clicked()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->tableViewRole->model());
    if (!model) {
        qWarning() << "model is't QStandardItemModel or nullptr";
        return;
    }

    const int checkColumn = 2; // 复选框所在列索引
    if (checkColumn < 0 || checkColumn >= model->columnCount()) {
        qWarning() << "checkColumn is nullptr" << model->columnCount();
        return;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        QStandardItem *item = model->item(row, checkColumn);
        if (item && item->isCheckable()) {
            item->setCheckState(Qt::Unchecked);
        }
    }

    ui->tableViewRole->viewport()->update();
}

void CheckEMP::on_pushButtonAll_clicked()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->tableViewRole->model());
    if (!model) {
        qWarning() << "model is't QStandardItemModel or nullptr";
        return;
    }

    const int checkColumn = 2; // 复选框所在列索引
    if (checkColumn < 0 || checkColumn >= model->columnCount()) {
        qWarning() << "checkColumn is nullptr" << model->columnCount();
        return;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        QStandardItem *item = model->item(row, checkColumn);
        if (item && item->isCheckable()) {
            item->setCheckState(Qt::Checked);
        }
    }

    ui->tableViewRole->viewport()->update();
}

void CheckEMP::on_lineEditEmp_returnPressed()
{
    QString empNo = ui->lineEditEmp->text().trimmed();
    ManagerSajet::is_Emp(empNo);
    if (!ManagerSajet::is_Emp(empNo)) {
        QMessageBox::critical(this, tr("错误"), tr("未找到工号"));
        return;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "数据库连接无效";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT R.ROLE_ID FROM SAJET.SYS_ROLE_EMP R "
                  "WHERE R.EMP_ID = (SELECT EMP_ID FROM SAJET.SYS_EMP WHERE EMP_NO = :eno)");
    query.bindValue(":eno", empNo);
    if (!query.exec()) {
        qWarning() << "查询角色失败:" << query.lastError().text();
        return;
    }

    QAbstractItemModel *roleModel = ui->tableViewRole->model();
    if (!roleModel) {
        qWarning() << "角色表格模型为空，请先调用 loadRole()";
        return;
    }

    int checkColumn = 2;
    for (int row = 0; row < roleModel->rowCount(); ++row) {
        QModelIndex checkIndex = roleModel->index(row, checkColumn);
        if (!checkIndex.isValid()) continue;

        int roleId = roleModel->data(checkIndex, Qt::UserRole).toInt();

        bool hasRole = m_roleIds.contains(roleId);
        Qt::CheckState state = hasRole ? Qt::Checked : Qt::Unchecked;

        // 更新复选框状态（需将模型转为 QStandardItemModel）
        QStandardItemModel *stdModel = qobject_cast<QStandardItemModel*>(roleModel);
        if (stdModel) {
            QStandardItem *item = stdModel->item(row, checkColumn);
            if (item) {
                item->setCheckState(state);
            }
        } else {
            roleModel->setData(checkIndex, state, Qt::CheckStateRole);
        }
    }
}

void CheckEMP::on_pushButtonSubmit_clicked()
{
    QString empNo = ui->lineEditEmpNo->text().trimmed();
    QString empName = ui->lineEditEmpName->text().trimmed();
    QString email = ui->lineEditEmail->text().trimmed();
    QString desc = ui->lineEditDesc->text().trimmed();
    QString dept = ui->comboBoxDept->currentText().trimmed();
    bool is_quit = ui->checkBox->isChecked();
    if (empNo.isEmpty()||empName.isEmpty()) {
        QMessageBox::critical(this,tr("错误"),tr("工号和名称不能为空"));
        return;
    }
    bool isEmp = ManagerSajet::is_Emp(empNo);
    if(isEmp){

    }else{
        if(is_quit){

        }
    }
}

