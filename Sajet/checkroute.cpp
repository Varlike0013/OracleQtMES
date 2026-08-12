#include "checkroute.h"
#include "ui_checkroute.h"
#include <QSqlDatabase>
#include "oracle_manager.h"
#include <QStandardItemModel>
#include <QMessageBox>
#include <QCheckBox>
#include <QSqlQuery>
#include <QInputDialog>

CheckRoute::CheckRoute(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CheckRoute)
{
    ui->setupUi(this);

    // 设置表格列数及表头
    ui->tableWidget->setColumnCount(4);
    QStringList headers;
    headers << tr("站位段") << tr("站位名称") << tr("站位ID") << tr("选择");
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 加载数据
    loadProcessData();
    loadUsefulRoutes();
}

CheckRoute::~CheckRoute()
{
    delete ui;
}

void CheckRoute::loadProcessData()
{
    // 固定的流程名称列表（与 Python 一致）
    QStringList processList = {
        "AOI", "BAOI", "BSPI", "BSVI", "PCB_INPUT", "SMT_INPUT", "SPI", "SVI",
        "S_AOI", "BottomVI", "CHANGE_SN", "CHECK_BAT", "CHECK_CPU", "CHECK_FAN",
        "DAOI", "DICT", "DInput", "DOA_VI", "FQC-CHK", "HEATSINK", "MDA", "PLATE",
        "TopVI", "CHK_LABEL1", "Cutboard", "F1Test", "F2Test", "F3Test", "F4Test",
        "GLUE_SVI", "Power On Test", "CHECKSN", "CHECK_BOX", "CHKPART", "CHKSSN",
        "CHK_MAC", "CHK_PART", "CQC", "ColorCheck", "OQC", "OQC_F1", "PACKING",
        "PBottomVI", "PK_AOI", "PK_VBATT", "PTopVI", "Packing1", "PrintLabel",
        "QC_CHK", "SOCPT0PVl"
    };

    // 生成占位符 :p0, :p1, ...
    QStringList placeholders;
    for (int i = 0; i < processList.size(); ++i) {
        placeholders << QString(":p%1").arg(i);
    }
    QString sql = QString("SELECT S.STAGE_NAME, P.PROCESS_NAME, P.PROCESS_ID "
                          "FROM SAJET.SYS_PROCESS P "
                          "JOIN SAJET.SYS_STAGE S ON S.STAGE_ID = P.STAGE_ID "
                          "WHERE P.ENABLED = 'Y' AND P.PROCESS_NAME IN (%1) "
                          "ORDER BY P.STAGE_ID, P.PROCESS_NAME")
                      .arg(placeholders.join(","));

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QSqlQuery query(db);
    query.prepare(sql);
    for (int i = 0; i < processList.size(); ++i) {
        query.bindValue(QString(":p%1").arg(i), processList[i]);
    }

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    // 清空表格
    ui->tableWidget->setRowCount(0);

    int row = 0;
    while (query.next()) {
        ui->tableWidget->insertRow(row);

        // 数据列
        QString stageName = query.value(0).toString(); // STAGE_NAME
        QString processName = query.value(1).toString(); // PROCESS_NAME
        QString processId = query.value(2).toString(); // PROCESS_ID

        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(stageName));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(processName));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(processId));
        QCheckBox *checkBox = new QCheckBox();
        checkBox->setTristate(true);
        checkBox->setCheckState(Qt::Unchecked);
        ui->tableWidget->setCellWidget(row, 3, checkBox);
        row++;
    }

    ui->tableWidget->resizeColumnsToContents();
}
void CheckRoute::loadUsefulRoutes()
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT ROUTE_NAME FROM SAJET.SYS_ROUTE WHERE ENABLED = 'Y' ORDER BY ROUTE_NAME";
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qDebug() << "LoadUsefulRoutes error:" << query.lastError().text();
        QMessageBox::warning(this, tr("警告"), tr("加载途程失败"));
        return;
    }

    ui->treeWidget->clear();
    while (query.next()) {
        QString routeName = query.value(0).toString();
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, routeName);
        item->setToolTip(0, routeName);   // 悬停提示
    }
    ui->treeWidget->expandAll();          // 展开所有节点
}
void CheckRoute::on_lineEdit_returnPressed()
{
    QString filter = ui->lineEdit->text().trimmed();
    filterTree(filter);
}
void CheckRoute::filterTree(const QString &filter)
{
    bool hasFilter = !filter.isEmpty();
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem(i);
        if (!item) continue;
        if (hasFilter) {
            bool match = item->text(0).contains(filter, Qt::CaseInsensitive);
            item->setHidden(!match);
        } else {
            item->setHidden(false);
        }
    }
}
void CheckRoute::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;
    QString routeName = item->text(0);
    if (routeName.isEmpty()) return;
    ui->labelRoute->setText(routeName);
    QList<RouteStep> steps = getRouteSteps(routeName);
    displaySteps(ui->listView, steps, true);
}
QList<RouteStep> CheckRoute::getRouteSteps(const QString &routeName)
{
    QList<RouteStep> steps;
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid";
        return steps;
    }

    QString sql = "SELECT U.PROCESS_NAME, D.NECESSARY "
                  "FROM SAJET.SYS_ROUTE_DETAIL D "
                  "INNER JOIN SAJET.SYS_ROUTE R ON D.ROUTE_ID = R.ROUTE_ID "
                  "INNER JOIN SAJET.SYS_PROCESS U ON D.NEXT_PROCESS_ID = U.PROCESS_ID "
                  "WHERE R.ROUTE_NAME = :route_name AND SEQ = STEP "
                  "ORDER BY D.SEQ ASC";
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":route_name", routeName);

    if (!query.exec()) {
        qWarning() << "Failed to get route steps:" << query.lastError().text();
        return steps;
    }

    while (query.next()) {
        RouteStep step;
        step.processName = query.value("PROCESS_NAME").toString();
        step.necessary = query.value("NECESSARY").toString().trimmed();
        steps.append(step);
    }
    return steps;
}
void CheckRoute::displaySteps(QListView *listView, const QList<RouteStep> &steps, bool colorByNecessary)
{
    QStandardItemModel *model = new QStandardItemModel(listView);
    for (const RouteStep &step : steps) {
        QStandardItem *item = new QStandardItem(step.processName);
        if (colorByNecessary) {
            if (step.necessary == "Y") {
                item->setForeground(QBrush(Qt::blue));
            } else if (step.necessary == "N") {
                item->setForeground(QBrush(Qt::red));
            }
        }
        model->appendRow(item);
    }
    listView->setModel(model);
}
void CheckRoute::on_pushButton_clicked()
{
    QStringList mustHave, mustNot;
    int rows = ui->tableWidget->rowCount();

    for (int row = 0; row < rows; ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->tableWidget->cellWidget(row, 3));
        if (!checkBox) continue;

        Qt::CheckState state = checkBox->checkState();
        QString processName = ui->tableWidget->item(row, 1)->text(); // 站位名称

        if (state == Qt::Checked) {
            mustHave << processName;
        } else if (state == Qt::PartiallyChecked) {
            mustNot << processName;
        }
    }

    if (mustHave.isEmpty() && mustNot.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请至少选择一个流程（必过或必不过）"));
        return;
    }

    fetchRouteProcess(mustHave, mustNot);
}
void CheckRoute::fetchRouteProcess(const QStringList &mustHave, const QStringList &mustNot)
{
    // 1. 检查必过列表是否为空
    if (mustHave.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("必须选择至少一个必过工序"));
        return;
    }

    // 2. 获取数据库连接
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 3. 转义并拼接列表（防止 SQL 注入）
    auto quoteAndJoin = [](const QStringList &list) -> QString {
        if (list.isEmpty()) return "''";  // 返回空字符串（不会匹配任何值）
        QStringList quoted;
        for (const QString &s : list) {
            QString safe = s;
            safe.replace("'", "''");
            quoted << "'" + safe + "'";
        }
        return quoted.join(",");
    };

    QString mustHaveStr = quoteAndJoin(mustHave);
    QString mustNotStr = quoteAndJoin(mustNot);
    int mustCount = mustHave.size();

    // 4. 构造 SQL（完全等价于存储过程逻辑）
    QString sql = QString(
                      "SELECT R.ROUTE_NAME "
                      "FROM SAJET.SYS_ROUTE R "
                      "INNER JOIN SAJET.SYS_ROUTE_DETAIL D ON R.ROUTE_ID = D.ROUTE_ID "
                      "INNER JOIN SAJET.SYS_PROCESS P ON D.PROCESS_ID = P.PROCESS_ID "
                      "GROUP BY R.ROUTE_NAME "
                      "HAVING "
                      "    COUNT(DISTINCT CASE WHEN P.PROCESS_NAME IN (%1) THEN P.PROCESS_NAME END) = %2 "
                      "    AND COUNT(DISTINCT CASE WHEN P.PROCESS_NAME IN (%3) THEN P.PROCESS_NAME END) = 0 "
                      "ORDER BY R.ROUTE_NAME"
                      ).arg(mustHaveStr)
                      .arg(mustCount)
                      .arg(mustNotStr);

    qDebug() << "Executing SQL:" << sql;

    // 5. 执行查询
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->treeWidget->clear();
    int count = 0;
    while (query.next()) {
        QString routeName = query.value(0).toString();
        QTreeWidgetItem *childItem = new QTreeWidgetItem(ui->treeWidget);
        childItem->setText(0, routeName);
        count++;
    }

    if (count == 0) {
        QMessageBox::information(this, tr("提示"), tr("未找到符合条件的路由"));
    }
}
void CheckRoute::on_pushButtonCopy_clicked()
{
    // 1. 获取原流程名称
    QString oldRoute = ui->labelRoute->text().trimmed();
    if (oldRoute.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个流程"));
        return;
    }

    // 2. 获取当前用户工号
    QString emp = OracleManager::instance().getCurrentUsername();
    if (emp.isEmpty()) {
        QMessageBox::critical(this, tr("错误"), tr("无法获取当前用户信息"));
        return;
    }

    // 3. 弹出输入框获取新流程名称
    bool ok;
    QString newRoute = QInputDialog::getText(this, tr("复制流程"),
                                             tr("请输入新流程名称:"),
                                             QLineEdit::Normal,
                                             oldRoute + "_copy", &ok);
    if (!ok || newRoute.trimmed().isEmpty()) {
        return; // 用户取消或未输入
    }
    newRoute = newRoute.trimmed();

    // 4. 询问是否覆盖（可选）
    int reply = QMessageBox::question(this, tr("覆盖确认"),
                                      tr("如果新流程已存在，是否覆盖?"),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply == QMessageBox::Cancel) {
        return;
    }
    QString overwriteFlag = (reply == QMessageBox::Yes) ? "Y" : "N";

    // 5. 调用存储过程
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QSqlQuery query(db);
    QString result;
    result.resize(100);
    query.prepare("BEGIN SAJET.SJ_COPY_ROUTE(:old, :new, :emp, :overwrite, :result); END;");
    query.bindValue(":old", oldRoute);
    query.bindValue(":new", newRoute);
    query.bindValue(":emp", emp);
    query.bindValue(":overwrite", overwriteFlag);
    query.bindValue(":result", result, QSql::Out);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("调用存储过程失败: %1").arg(query.lastError().text()));
        return;
    }

    result = query.boundValue(":result").toString();
    QMessageBox::information(this, tr("结果"), result);
}
void CheckRoute::on_pushButtonFlash_clicked()
{
    loadUsefulRoutes();
}

