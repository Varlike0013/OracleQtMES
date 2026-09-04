#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "oracle_manager.h"
#include "logindialog.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QDebug>
#include <QTimer>
#include <QTableView>
#include <QPushButton>
#include <QInputDialog>
#include <QTextEdit>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 建立数据库连接
    if (!initDatabaseConnection()) {
        // 连接失败，可以关闭窗口或显示错误信息
        QMessageBox::critical(this, "连接失败",
                              "无法连接到数据库，程序将退出。");
        // 延迟关闭，让消息框显示
        QTimer::singleShot(0, this, &QMainWindow::close);
        return;
    }

    setupUI(); // 初始化界面显示
    loadDatabaseTree();//加载数据库全部表
}

MainWindow::~MainWindow()
{
    // 如果连接还开着，关闭它
    if (m_db.isOpen()) {
        m_db.close();
    }

    delete ui;
}
void MainWindow::setupUI()
{
    // 在窗口标题或状态栏显示用户名
    setWindowTitle(QString("数据库管理 - %1").arg(m_connInfo.key));
    setStatusMessage("当前用户: " + m_username);
}
void MainWindow::setStatusMessage(const QString &msg)
{
    ui->statusbar->showMessage(msg);
}
bool MainWindow::initDatabaseConnection()
{
    OracleManager &mgr = OracleManager::instance();
    m_connInfo = mgr.getCurrentDbInfo();
    m_username = mgr.getCurrentUsername();
    DbConnectionResult result = mgr.connectDatabase(m_connInfo, mainConnect);
    m_db = result.database;
    if (result.success) {
        // 保存数据库对象
        m_db = result.database;
        qDebug() << "数据库连接成功！主机：" << m_connInfo.host
                 << "服务名：" << m_connInfo.serviceName;
        ui->statusbar->showMessage("已连接到: " + m_connInfo.serviceName);
        return true;
    } else {
        qWarning() << "连接失败：" << result.errorMessage;
        ui->statusbar->showMessage("连接失败");
        return false;
    }
}
void MainWindow::loadDatabaseTree()
{
    if (!m_db.isOpen()) {
        QMessageBox::warning(this, "错误", "数据库未连接");
        return;
    }

    ui->treeObjects->clear();

    // 创建顶级分类节点
    QTreeWidgetItem *tableRoot = new QTreeWidgetItem(ui->treeObjects);
    tableRoot->setText(0, "Table");
    QTreeWidgetItem *procRoot = new QTreeWidgetItem(ui->treeObjects);
    procRoot->setText(0, "Procedure");

    // 缓存节点（避免重复查找）
    QMap<QString, QTreeWidgetItem*> schemaCache;
    QMap<QString, QTreeWidgetItem*> prefixCache;

    // ---- 加载表 ----
    QSqlQuery tableQuery(m_db);
    if (tableQuery.exec("SELECT owner, table_name FROM all_tables "
                        "WHERE owner NOT IN ('SYS','SYSTEM','DBSNMP','XDB','WMSYS','OUTLN','APPQOSSYS','ORACLE_OCM') "
                        "ORDER BY owner, table_name")) {
        while (tableQuery.next()) {
            QString owner = tableQuery.value(0).toString().trimmed();
            QString tableName = tableQuery.value(1).toString().trimmed();
            addDatabaseObject(tableRoot, owner, tableName, schemaCache, prefixCache);
        }
    } else {
        qWarning() << "SELECT TABLE ERR:" << tableQuery.lastError().text();
    }
    // ---- 加载存储过程 ----
    QSqlQuery procQuery(m_db);
    if (procQuery.exec("SELECT owner, object_name FROM all_procedures "
                       "WHERE object_type = 'PROCEDURE' AND owner NOT IN ('SYS','SYSTEM','DBSNMP','XDB','WMSYS','OUTLN','APPQOSSYS','ORACLE_OCM') "
                       "ORDER BY owner, object_name")) {
        while (procQuery.next()) {
            QString owner = procQuery.value(0).toString().trimmed();
            QString procName = procQuery.value(1).toString().trimmed();
            addDatabaseObject(procRoot, owner, procName, schemaCache, prefixCache);
        }
    } else {
        qWarning() << "SELECT PROCEDURE ERR:" << procQuery.lastError().text();
    }
    // ---- 排序 ----
    ui->treeObjects->sortItems(0, Qt::AscendingOrder);
}
// 获取或创建 Schema 节点
QTreeWidgetItem* MainWindow::getSchemaNode(QTreeWidgetItem *root, const QString &schema)
{
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem *child = root->child(i);
        if (child->text(0) == schema) {
            return child;
        }
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(root);
    item->setText(0, schema);
    return item;
}

// 获取或创建前缀节点（第三层）
QTreeWidgetItem* MainWindow::getPrefixNode(QTreeWidgetItem *schemaItem, const QString &prefix)
{
    for (int i = 0; i < schemaItem->childCount(); ++i) {
        QTreeWidgetItem *child = schemaItem->child(i);
        if (child->text(0) == prefix) {
            return child;
        }
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(schemaItem);
    item->setText(0, prefix);
    return item;
}

// 通用添加对象函数
void MainWindow::addDatabaseObject(QTreeWidgetItem *root,
                                   const QString &schema,
                                   const QString &fullName,
                                   QMap<QString, QTreeWidgetItem*> &schemaCache,
                                   QMap<QString, QTreeWidgetItem*> &prefixCache)
{
    // 提取前缀
    QString prefix = fullName;
    int idx = fullName.indexOf('_');
    if (idx != -1) {
        prefix = fullName.left(idx);
    } else {
        prefix = "其他";
    }

    // 获取或创建 Schema 节点（第二层）
    QTreeWidgetItem *schemaItem = getSchemaNode(root, schema);

    // 获取或创建前缀节点（第三层）
    QTreeWidgetItem *prefixItem = getPrefixNode(schemaItem, prefix);

    // 创建叶子节点（第四层）
    QTreeWidgetItem *leaf = new QTreeWidgetItem(prefixItem);
    leaf->setText(0, fullName);                         // 显示名称
    leaf->setData(0, Qt::UserRole, fullName);          // 完整对象名（不含 schema）
    leaf->setData(0, Qt::UserRole + 1, schema);        // 所属 schema
}
void MainWindow::on_searchEdit_returnPressed()
{
    QString text = ui->searchEdit->text();
    QString searchText = text.trimmed();
    bool hasSearch = !searchText.isEmpty();

    // 遍历顶层分类节点（"表"、"存储过程"）
    for (int i = 0; i < ui->treeObjects->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = ui->treeObjects->topLevelItem(i);
        if (!category) continue;

        // 递归设置可见性
        setItemVisibility(category, searchText, hasSearch);

        // 展开/折叠分类节点
        if (hasSearch) {    // 如果有可见子节点则展开，否则折叠
            category->setExpanded(hasVisibleChild(category));
        } else {    // 无搜索时全部折叠
            category->setExpanded(false);
        }
    }
}
void MainWindow::setItemVisibility(QTreeWidgetItem *item, const QString &searchText, bool hasSearch)
{
    if (!item) return;

    bool isLeaf = (item->childCount() == 0);

    if (isLeaf) {
        // 叶子节点：匹配全名（不区分大小写）
        if (hasSearch) {
            QString name = item->text(0);
            bool match = name.contains(searchText, Qt::CaseInsensitive);
            item->setHidden(!match);
        } else {
            item->setHidden(false); // 无搜索时全部显示
        }
    } else {
        // 非叶子节点：先递归处理所有子节点
        for (int i = 0; i < item->childCount(); ++i) {
            setItemVisibility(item->child(i), searchText, hasSearch);
        }
        // 根据是否有可见子节点决定自身是否隐藏
        bool hasVisible = hasVisibleChild(item);
        item->setHidden(!hasVisible);

        // 如果处于搜索模式且自身可见，则展开（显示路径）
        if (hasSearch) {
            item->setExpanded(hasVisible);
        } else {
            item->setExpanded(true);
        }
    }
}
bool MainWindow::hasVisibleChild(QTreeWidgetItem *item)
{
    if (!item) return false;
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (!child->isHidden()) {
            return true;
        }
        if (hasVisibleChild(child)) {
            return true;
        }
    }
    ui->tabWidget->clear();
    return false;
}
void MainWindow::on_treeObjects_itemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    if (!item || item->childCount() > 0) return;

    QString fullName = item->data(0, Qt::UserRole).toString();
    QString schema = item->data(0, Qt::UserRole + 1).toString();
    if (fullName.isEmpty() || schema.isEmpty()) {
        qWarning() << "叶子节点缺少数据";
        return;
    }

    // 判断是表还是存储过程（通过祖先节点判断）
    QString type = getObjectType(item);
    if (type.isEmpty()) {
        qWarning() << "无法确定对象类型";
        return;
    }

    // 构建 tab 标题（如 "表: SAJET.EMP"）
    QString tabTitle = QString("%1: %2.%3").arg(type).arg(schema).arg(fullName);

    // 检查是否已存在相同 tab
    int index = findTabIndex(tabTitle);
    if (index != -1) {
        ui->tabWidget->setCurrentIndex(index);
        return;
    }

    // 根据类型创建不同的内容页面
    if (type == "Table") {
        showTableData(schema, fullName);
    } else if (type == "Procedure") {
        showProcedureCode(schema, fullName);
    } else {
        qWarning() << "未知对象类型:" << type;
    }
}
int MainWindow::findTabIndex(const QString &title)
{
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (ui->tabWidget->tabText(i) == title) {
            return i;
        }
    }
    return -1;
}
QString MainWindow::getObjectType(QTreeWidgetItem *item)
{
    if (!item) return QString();
    QTreeWidgetItem *parent = item->parent();
    while (parent && parent->parent()) {
        parent = parent->parent();
    }
    if (parent) {
        return parent->text(0);
    }
    return QString();
}
void MainWindow::showTableData(const QString &schema, const QString &tableName)
{
    // 创建新的 tab 页面
    QWidget *tabPage = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tabPage);
    layout->setContentsMargins(2, 2, 2, 2);

    // 创建表格视图
    QTableView *view = new QTableView(tabPage);
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);// 1. 列宽自适应内容（并允许用户调整）
    view->horizontalHeader()->setStretchLastSection(true);// 2. 最后一列拉伸填满剩余空间（避免右侧空白）
    // view->verticalHeader()->setVisible(false);// 3. 垂直头（行号）默认显示，可不调整，也可隐藏

    // 使用 QSqlTableModel 加载数据
    QSqlTableModel *model = new QSqlTableModel(tabPage, m_db);
    // 如果 schema 不是当前用户，需要设置表名为 "schema.table"
    QString tableFullName = schema + "." + tableName;
    model->setTable(tableFullName);
    model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    model->setSort(-1, Qt::AscendingOrder);
    model->setFilter("ROWNUM <= 100");
    if (!model->select()) {
        QMessageBox::warning(this, "加载失败",
                             QString("无法加载表 %1\n%2").arg(tableFullName).arg(model->lastError().text()));
        delete tabPage;
        return;
    }
    view->setModel(model);
    layout->addWidget(view);

    // 连接表头的点击信号
    QHeaderView *header = view->horizontalHeader();
    connect(header, &QHeaderView::sectionClicked, this, [this, view, model](int logicalIndex) {
        // logicalIndex 是列的逻辑索引（按模型列顺序）
        QString columnName = model->headerData(logicalIndex, Qt::Horizontal).toString();
        // 弹出输入框
        QString filterText = QInputDialog::getText(this, "列过滤",
                                                   QString("请输入要在 '%1' 列中搜索的值：").arg(columnName));
        if (filterText.isEmpty()) {
            // 如果为空，则取消过滤
            model->setFilter(QString());
            return;
        }
        // 构建过滤条件（Oracle 语法）
        // 注意：若列名包含特殊字符（如空格、特殊符号），需用双引号括起来
        QString filter = QString("%1 = '%2'").arg(columnName).arg(filterText);
        model->setFilter(filter);
        model->select();  // 重新查询
    });
    // 添加操作按钮（可选）
    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *btnRefresh = new QPushButton("刷新", tabPage);
    QPushButton *btnQuery   = new QPushButton("查询", tabPage);
    QPushButton *btnSubmit = new QPushButton("提交", tabPage);
    QPushButton *btnRevert = new QPushButton("回滚", tabPage);
    QPushButton *btnAddLine = new QPushButton("添加", tabPage);
    btnLayout->addWidget(btnQuery);
    btnLayout->addWidget(btnRefresh);
    btnLayout->addWidget(btnSubmit);
    btnLayout->addWidget(btnRevert);
    btnLayout->addWidget(btnAddLine);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // 连接按钮信号（简单实现）
    connect(btnQuery, &QPushButton::clicked, [=]() {
        tabwidget_query_button();
    });
    connect(btnAddLine, &QPushButton::clicked, [=]() {
        // 弹出确认框，提示用户谨慎操作
        int reply = QMessageBox::question(
            this,
            "添加新行",
            "确定要在当前表中添加新行吗？\n\n"
            "建议：尽量避免在数据库中直接添加数据，\n"
            "若需添加，请确认数据正确性后再提交。\n\n"
            "是否继续添加？",
            QMessageBox::Yes | QMessageBox::No
            );
        if (reply != QMessageBox::Yes) {
            return; // 用户取消，不执行添加
        }
        // 插入新行（末尾）
        int row = model->rowCount();
        if (!model->insertRow(row)) {
            QMessageBox::critical(this, "添加失败", model->lastError().text());
            return;
        }
        // 选中新行并滚动到可见区域
        QModelIndex index = model->index(row, 0);
        view->setCurrentIndex(index);
        view->scrollTo(index);
    });
    connect(btnRefresh, &QPushButton::clicked, [=]() {
        model->select();
    });
    connect(btnSubmit, &QPushButton::clicked, [=]() {
        if (model->submitAll()) {
            model->select();
            QMessageBox::information(this, "成功", "数据已提交");
        } else {
            QMessageBox::critical(this, "提交失败", model->lastError().text());
        }
    });
    connect(btnRevert, &QPushButton::clicked, [=]() {
        model->revertAll();
        QMessageBox::information(this, "回滚", "所有修改已撤销");
    });

    // 添加 tab
    QString tabTitle = QString("Table: %1.%2").arg(schema).arg(tableName);
    int index = ui->tabWidget->addTab(tabPage, tabTitle);
    ui->tabWidget->setCurrentIndex(index);
}
void MainWindow::showProcedureCode(const QString &schema, const QString &procName)
{
    // 创建新的 tab 页面
    QWidget *tabPage = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tabPage);
    layout->setContentsMargins(2, 2, 2, 2);

    // 文本编辑框（只读）
    QTextEdit *textEdit = new QTextEdit(tabPage);
    textEdit->setFont(QFont("Courier New", 10));
    textEdit->setReadOnly(true);

    // 查询存储过程源代码（Oracle）
    QSqlQuery query(m_db);
    // 使用 all_source 以便跨 schema 查询
    query.prepare("SELECT text FROM all_source "
                  "WHERE owner = :owner AND name = :name AND type = 'PROCEDURE' "
                  "ORDER BY line");
    query.bindValue(":owner", schema);
    query.bindValue(":name", procName);

    QString code;
    if (query.exec()) {
        while (query.next()) {
            code += query.value(0).toString();
        }
        if (code.isEmpty()) {
            code = "（未找到存储过程源代码）";
        }
    } else {
        code = "查询源代码失败: " + query.lastError().text();
    }

    textEdit->setPlainText(code);
    layout->addWidget(textEdit);

    // 添加 tab
    QString tabTitle = QString("Procedure: %1.%2").arg(schema).arg(procName);
    int index = ui->tabWidget->addTab(tabPage, tabTitle);
    ui->tabWidget->setCurrentIndex(index);
}
void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    QWidget *tabPage = ui->tabWidget->widget(index);
    if (!tabPage) return;
    ui->tabWidget->removeTab(index);
    delete tabPage;
}

void MainWindow::on_actionExit_triggered()
{
    if (QMessageBox::question(this, "确认退出", "确定要退出程序吗？") == QMessageBox::Yes) {
        QApplication::quit();
    }
}
void MainWindow::on_actionSubmit_triggered()
{
    QWidget *currentTab = ui->tabWidget->currentWidget();
    if (!currentTab) {
        QMessageBox::warning(this, "提交", "没有活动的标签页");
        return;
    }

    // 查找当前页中的表格视图
    QTableView *view = currentTab->findChild<QTableView*>();
    if (!view) {
        QMessageBox::warning(this, "提交", "当前标签页不是表格数据页");
        return;
    }

    QSqlTableModel *model = qobject_cast<QSqlTableModel*>(view->model());
    if (!model) {
        QMessageBox::warning(this, "提交", "当前表格没有有效的模型");
        return;
    }

    if (model->submitAll()) {
        model->select();
        QMessageBox::information(this, "提交", "数据提交成功");
    } else {
        QMessageBox::critical(this, "提交失败", model->lastError().text());
    }
}

void MainWindow::on_actionRevert_triggered()
{
    QWidget *currentTab = ui->tabWidget->currentWidget();
    if (!currentTab) {
        QMessageBox::warning(this, "回滚", "没有活动的标签页");
        return;
    }

    QTableView *view = currentTab->findChild<QTableView*>();
    if (!view) {
        QMessageBox::warning(this, "回滚", "当前标签页不是表格数据页");
        return;
    }

    QSqlTableModel *model = qobject_cast<QSqlTableModel*>(view->model());
    if (!model) {
        QMessageBox::warning(this, "回滚", "当前表格没有有效的模型");
        return;
    }

    model->revertAll();
    model->select(); // 刷新显示
    QMessageBox::information(this, "回滚", "所有修改已撤销");
}
void MainWindow::on_actionQuery_triggered()
{
    // 弹出输入框，让用户输入sql语句
    bool ok;
    QString sql  = QInputDialog::getMultiLineText(
        this,
        "条件查询",
        QString("请输入 完整sql 条件：\n\n"
                "示例: SELECT * FROM schema.table WHERE colnum = 'value'\n"),
        "",
        &ok
        );

    if (!ok) {
        return; // 用户取消了
    }

    if (!ok || sql.trimmed().isEmpty()) {
        return;
    }

    // 执行查询并显示结果
    QString displayTitle = sql.trimmed().left(50);
    if (sql.length() > 50) displayTitle += "...";
    executeQueryAndShowResult(sql, displayTitle,"");
}
void MainWindow::tabwidget_query_button()
{
    // 1. 获取当前活动 tab 页
    QWidget *currentTab = ui->tabWidget->currentWidget();
    if (!currentTab) {
        QMessageBox::warning(this, "查询", "没有活动的标签页");
        return;
    }

    // 2. 从 tab 标题中解析表名和 schema
    QString tabTitle = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
    // 表 tab 标题格式："表: SAJET.EMP" 或 "表: SAJET.G_SN_TRAVEL"
    if (!tabTitle.startsWith("Table: ")) {
        QMessageBox::warning(this, "查询", "当前标签页不是表数据页，请切换到表页");
        return;
    }

    QString fullTableName = tabTitle.mid(6).trimmed(); // 去掉 "表: "
    // 检查是否包含 '.', 分割 schema 和 table
    QString schema, tableName;
    if (fullTableName.contains('.')) {
        QStringList parts = fullTableName.split('.');
        schema = parts[0];
        tableName = parts[1];
    } else {
        // 如果没有 schema，使用当前用户
        schema = m_db.userName();
        tableName = fullTableName;
    }

    // 3. 弹出输入框，让用户输入 WHERE 条件
    bool ok;
    QString condition = QInputDialog::getMultiLineText(
        this,
        "条件查询",
        QString("请输入 WHERE 条件（无需输入 WHERE 关键字）：\n\n"
                "当前表: %1.%2\n\n"
                "示例: SN = 'ABC123' AND WK = '456-S'\n")
            .arg(schema)
            .arg(tableName),
        "", // 默认空
        &ok
        );

    if (!ok) {
        return; // 用户取消了
    }

    // 4. 构建完整的 SQL 语句
    QString fullSql;
    if (condition.trimmed().isEmpty()) {
        fullSql = QString("SELECT * FROM %1.%2 WHERE ROWNUM <= 100")
                      .arg(schema)
                      .arg(tableName);
    } else {
        fullSql = QString("SELECT * FROM %1.%2 T WHERE %3")
                      .arg(schema)
                      .arg(tableName)
                      .arg(condition);
    }

    qDebug() << "执行查询：" << fullSql;

    // 5. 执行查询并显示结果
    executeQueryAndShowResult(fullSql, fullTableName, condition.trimmed().isEmpty() ? "全部" : condition);
}
void MainWindow::executeQueryAndShowResult(const QString &sql, const QString &tableName, const QString &condition)
{
    // 创建新的 tab 页
    QWidget *tabPage = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(tabPage);
    layout->setContentsMargins(2, 2, 2, 2);

    // 创建表格视图
    QTableView *view = new QTableView(tabPage);
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view->horizontalHeader()->setStretchLastSection(true);

    // 使用 QSqlQueryModel 执行查询
    QSqlQueryModel *model = new QSqlQueryModel(tabPage);
    model->setQuery(sql, m_db);

    if (model->lastError().isValid()) {
        QMessageBox::critical(this, "查询失败", model->lastError().text());
        delete tabPage;
        return;
    }

    view->setModel(model);
    layout->addWidget(view);

    // 添加关闭按钮（可选）
    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *btnClose = new QPushButton("关闭此页", tabPage);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    connect(btnClose, &QPushButton::clicked, [this, tabPage]() {
        int index = ui->tabWidget->indexOf(tabPage);
        if (index != -1) {
            ui->tabWidget->removeTab(index);
            delete tabPage;
        }
    });

    // 添加 tab
    QString tabTitle = QString("查询: %1 (%2)").arg(tableName).arg(condition.left(20));
    if (condition.length() > 20) tabTitle += "...";
    int index = ui->tabWidget->addTab(tabPage, tabTitle);
    ui->tabWidget->setCurrentIndex(index);
}
void MainWindow::on_actionchangedb_triggered()
{
    // 1. 关闭所有数据库连接（由 OracleManager 管理）
    OracleManager::instance().disconnectAll();

    // 2. 隐藏当前主窗口
    this->hide();

    // 3. 弹出登录对话框
    LoginDialog loginDlg;
    if (loginDlg.exec() == QDialog::Accepted) {
        // 登录成功，销毁旧窗口，创建新主窗口
        this->deleteLater();   // 安全地删除旧窗口
        MainWindow *newMain = new MainWindow();
        newMain->show();
    } else {
        // 用户取消，重新显示当前主窗口
        this->show();
    }
}

