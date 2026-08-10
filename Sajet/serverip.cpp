#include "serverip.h"
#include "ui_serverip.h"
#include "oracle_manager.h"
#include <qsqlquery.h>
#include <QMessageBox>
#include <qsqlquerymodel.h>
#include "managersajet.h"
#include <QFileDialog>
#include <QFutureWatcher>
#include <QStandardPaths>

ServerIp::ServerIp(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ServerIp)
{
    ui->setupUi(this);
    load_serveriptree();
    init_table_view();
}

ServerIp::~ServerIp()
{
    delete ui;
}
void ServerIp::load_serveriptree()
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    QString sql = "SELECT S.SERVER_ID, S.SERVER_DESC_E, G.GATEWAY_ID, G.DRIVER_ID, "
                  "G.GAYTEWAY_CONNECT_NUMBER, G.GATEWAY_DESC_E "
                  "FROM SAJET.TGS_SERVER_BASE S "
                  "INNER JOIN SAJET.TGS_GATEWAY_BASE G ON G.SERVER_ID = S.SERVER_ID "
                  "WHERE S.ENABLED = 'Y' AND G.ENABLED = 'Y' "
                  "ORDER BY S.SERVER_DESC_E";

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        QMessageBox::critical(this, tr("错误"), tr("加载服务器网关数据失败: %1").arg(query.lastError().text()));
        return;
    }

    ui->treeWidget->clear();

    // 用于缓存已添加的服务器节点（通过 SERVER_ID）
    QMap<QString, QTreeWidgetItem*> serverMap;

    while (query.next()) {
        QString serverId = query.value("SERVER_ID").toString();
        QString serverDesc = query.value("SERVER_DESC_E").toString();
        QString gatewayId = query.value("GATEWAY_ID").toString();
        QString driverId = query.value("DRIVER_ID").toString();
        QString gatewayConnectNumber = query.value("GAYTEWAY_CONNECT_NUMBER").toString();
        QString gatewayDesc = query.value("GATEWAY_DESC_E").toString();

        // 1. 查找或创建服务器节点（第一层）
        QTreeWidgetItem *serverItem = nullptr;
        if (serverMap.contains(serverId)) {
            serverItem = serverMap[serverId];
        } else {
            serverItem = new QTreeWidgetItem(ui->treeWidget);
            serverItem->setText(0, serverDesc+"("+serverId+")");
            serverItem->setData(0, Qt::UserRole, serverId); // 保存 SERVER_ID
            serverMap[serverId] = serverItem;
        }

        // 2. 创建网关节点（第二层）
        QTreeWidgetItem *gatewayItem = new QTreeWidgetItem(serverItem);
        QString displayText = gatewayDesc + "(" + gatewayId + ")"+"-"+gatewayConnectNumber;
        gatewayItem->setText(0, displayText);
        // 保存网关信息
        gatewayItem->setData(0, Qt::UserRole, serverId);
        gatewayItem->setData(0, Qt::UserRole + 1, gatewayId);
        gatewayItem->setData(0, Qt::UserRole + 2, driverId);
        gatewayItem->setData(0, Qt::UserRole + 3, gatewayConnectNumber);
    }

    // // 连接展开信号，用于动态加载第三层
    // connect(ui->treeWidget, &QTreeWidget::itemExpanded,
    //         this, &ServerIp::onGatewayExpanded);
    // // 但注意：此信号会在每次展开时触发，我们需要判断是否为网关节点
}
void ServerIp::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;

    // 判断节点层级
    QTreeWidgetItem *parent = item->parent();
    QTreeWidgetItem *grandParent = parent ? parent->parent() : nullptr;

    // 第三层节点（IP 节点）
    if (grandParent != nullptr && parent != nullptr) {
        QString ip = item->data(0, Qt::UserRole).toString();
        QString serverId = item->data(0, Qt::UserRole + 1).toString();
        QString gatewayId = item->data(0, Qt::UserRole + 2).toString();
        int index = item->data(0, Qt::UserRole + 3).toInt();

        if (serverId.isEmpty() || gatewayId.isEmpty()) {
            qWarning() << "IP node missing server/gateway ID";
            return;
        }
        append_table_row(serverId,gatewayId,index);

        return;
    }

    // 第二层节点（网关节点）- 动态加载 IP（如果未加载过）
    if (parent != nullptr && parent->parent() == nullptr) {
        // 如果已经加载过子节点（即已有子节点），则不重复加载
        if (item->childCount() > 0) {
            QString serverId = item->data(0, Qt::UserRole).toString();
            QString gatewayId = item->data(0, Qt::UserRole + 1).toString();
            int gatewayConnectNumber = item->data(0, Qt::UserRole + 3).toInt();
            for(int i=0;i<gatewayConnectNumber;i++){
                append_table_row(serverId,gatewayId,i);
            }
            return;
        }

        QString serverId = item->data(0, Qt::UserRole).toString();
        QString gatewayId = item->data(0, Qt::UserRole + 1).toString();
        QString driverId = item->data(0, Qt::UserRole + 2).toString();
        int gatewayConnectNumber = item->data(0, Qt::UserRole + 3).toInt();

        if (serverId.isEmpty() || gatewayId.isEmpty() || driverId.isEmpty()) {
            qWarning() << "Gateway node missing required data";
            return;
        }

        QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
        if (!db.isValid() || !db.isOpen()) {
            QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
            return;
        }

        // 调用存储过程获取 IP
        QString tres, ipStr;
        tres.reserve(100);
        ipStr.reserve(1000);

        QSqlQuery query(db);
        query.prepare("BEGIN SAJET.GET_GATEWAY_IP(:server, :gateway, :driver, :tres, :ip); END;");
        query.bindValue(":server", serverId);
        query.bindValue(":gateway", gatewayId);
        query.bindValue(":driver", driverId);
        query.bindValue(":tres", tres, QSql::Out);
        query.bindValue(":ip", ipStr, QSql::Out);

        if (!query.exec()) {
            QMessageBox::critical(this, tr("错误"), tr("调用存储过程失败: %1").arg(query.lastError().text()));
            return;
        }

        tres = query.boundValue(":tres").toString();
        ipStr = query.boundValue(":ip").toString();

        if (tres != "OK") {
            QMessageBox::warning(this, tr("警告"), tr("获取 IP 失败: %1").arg(tres));
            QTreeWidgetItem *errorItem = new QTreeWidgetItem(item);
            errorItem->setText(0, tr("获取IP失败: %1").arg(tres));
            return;
        }

        // 使用正则表达式提取所有 IPv4 地址
        QStringList ipList;
        QRegularExpression re(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
        QRegularExpressionMatchIterator it = re.globalMatch(ipStr);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            ipList << match.captured(0);
        }

        if (ipList.isEmpty()) {
            QTreeWidgetItem *emptyItem = new QTreeWidgetItem(item);
            emptyItem->setText(0, tr("无可用IP"));
            return;
        }

        // 创建 IP 子节点
        int idx = 0;
        for (const QString &ip : ipList) {
            QTreeWidgetItem *ipItem = new QTreeWidgetItem(item);
            ipItem->setText(0, QString("IP%1: %2(%3)").arg(idx + 1).arg(ip).arg(get_ip_status(serverId,gatewayId,idx)));
            ipItem->setData(0, Qt::UserRole, ip);
            ipItem->setData(0, Qt::UserRole + 1, serverId);
            ipItem->setData(0, Qt::UserRole + 2, gatewayId);
            ipItem->setData(0, Qt::UserRole + 3, idx);
            idx++;
        }

        item->setExpanded(true);
        return;
    }

    // 第一层节点（服务器）- 不做特殊处理
}
void ServerIp::init_table_view()
{
    // 若已有模型，删除旧模型
    if (m_tableModel) {
        delete m_tableModel;
        m_tableModel = nullptr;
    }

    // 创建新模型，设置列数（8列）
    m_tableModel = new QStandardItemModel(this);
    m_tableModel->setColumnCount(8);

    // 设置表头（与 SELECT 字段顺序一致）
    QStringList headers;
    headers << tr("站别") << tr("产线") << tr("网关描述")
            << tr("终端ID") << tr("终端名称") << tr("工序")
            << tr("组ID") << tr("启用状态");
    m_tableModel->setHorizontalHeaderLabels(headers);

    // 设置到表格视图
    ui->tableView->setModel(m_tableModel);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setVisible(false); // 隐藏行号
}
void ServerIp::on_pushButton_clicked()
{
    if (m_tableModel) {
        m_tableModel->removeRows(0, m_tableModel->rowCount());
    }
}
void ServerIp::append_table_row(QString serverId,QString gatewayId,int index)
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接无效"));
        return;
    }

    // 构建查询（使用参数绑定防止 SQL 注入）
    QString sql =
        "SELECT S.STAGE_NAME, PL.PDLINE_NAME, G.GATEWAY_DESC_E, "
        "T.TERMINAL_ID, T.TERMINAL_NAME, P.PROCESS_NAME, "
        "TL.GROUP_ID, T.ENABLED "
        "FROM SAJET.TGS_TERMINAL_LINK TL "
        "INNER JOIN SAJET.SYS_TERMINAL T ON TL.TERMINAL_ID = T.TERMINAL_ID "
        "INNER JOIN SAJET.SYS_PDLINE PL ON PL.PDLINE_ID = T.PDLINE_ID "
        "INNER JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = T.PROCESS_ID "
        "INNER JOIN SAJET.SYS_STAGE S ON S.STAGE_ID = T.STAGE_ID "
        "INNER JOIN SAJET.TGS_GATEWAY_BASE G ON G.GATEWAY_ID = TL.GATEWAY_ID AND G.SERVER_ID = TL.SERVER_ID "
        "WHERE TL.SERVER_ID = :server "
        "  AND TL.GATEWAY_ID = :gateway "
        "  AND TL.DEVICE_ID = :idx";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":server", serverId);
    query.bindValue(":gateway", gatewayId);
    query.bindValue(":idx", index);

    if (!query.exec()) {
        QMessageBox::critical(this, tr("错误"), tr("查询失败: %1").arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        QList<QStandardItem*> rowItems;
        for (int i = 0; i < 8; ++i) {
            QStandardItem *item = new QStandardItem(query.value(i).toString());
            item->setEditable(false);
            rowItems.append(item);
        }
        m_tableModel->appendRow(rowItems);
    }
    ui->tableView->resizeColumnsToContents();
}
QString ServerIp::get_ip_status(QString serverId, QString gatewayId, int index)
{
    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid in get_ip_status";
        return tr("数据库连接无效");
    }

    // 构建查询
    QString sql =
        "SELECT S.STAGE_NAME, PL.PDLINE_NAME, G.GATEWAY_DESC_E, "
        "T.TERMINAL_ID, T.TERMINAL_NAME, P.PROCESS_NAME, "
        "TL.GROUP_ID, T.ENABLED "
        "FROM SAJET.TGS_TERMINAL_LINK TL "
        "INNER JOIN SAJET.SYS_TERMINAL T ON TL.TERMINAL_ID = T.TERMINAL_ID "
        "INNER JOIN SAJET.SYS_PDLINE PL ON PL.PDLINE_ID = T.PDLINE_ID "
        "INNER JOIN SAJET.SYS_PROCESS P ON P.PROCESS_ID = T.PROCESS_ID "
        "INNER JOIN SAJET.SYS_STAGE S ON S.STAGE_ID = T.STAGE_ID "
        "INNER JOIN SAJET.TGS_GATEWAY_BASE G ON G.GATEWAY_ID = TL.GATEWAY_ID AND G.SERVER_ID = TL.SERVER_ID "
        "WHERE TL.SERVER_ID = :server "
        "  AND TL.GATEWAY_ID = :gateway "
        "  AND TL.DEVICE_ID = :idx";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":server", serverId);
    query.bindValue(":gateway", gatewayId);
    query.bindValue(":idx", index);

    if (!query.exec()) {
        qWarning() << "Query failed in get_ip_status:" << query.lastError().text();
        return tr("查询失败: %1").arg(query.lastError().text());
    }

    // 如果无结果，返回提示
    if (!query.next()) {
        return tr("无终端状态数据");
    }

    // 组合结果：第一行
    QStringList fields;
    fields  << query.value(4).toString();   // TERMINAL_NAME
            // << query.value(0).toString()   // STAGE_NAME
           // << query.value(1).toString()   // PDLINE_NAME
           // << query.value(2).toString()   // GATEWAY_DESC_E
           // << query.value(3).toString()   // TERMINAL_ID
           // << query.value(4).toString()   // TERMINAL_NAME
           // << query.value(5).toString()   // PROCESS_NAME
           // << query.value(6).toString()   // GROUP_ID
           // << query.value(7).toString();  // ENABLED

    QString result = fields.join(" | ");

    // 如果有多行，继续追加
    while (query.next()) {
        QStringList extraFields;
        extraFields << query.value(4).toString();
                    // << query.value(0).toString()
                    // << query.value(1).toString()
                    // << query.value(2).toString()
                    // << query.value(3).toString()
                    // << query.value(4).toString()
                    // << query.value(5).toString()
                    // << query.value(6).toString()
                    // << query.value(7).toString();
        result += "；" + extraFields.join(" | ");
    }

    return result;
}
void ServerIp::on_pushButtonOut_clicked()
{
    // 检查是否已有导出任务正在运行
    if (m_exportFuture.isRunning()) {
        QMessageBox::warning(this, tr("提示"), tr("导出中，请等待导出结果"));
        return;
    }

    // 设置默认路径：桌面 + 默认文件名
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString defaultFilePath = desktopPath + "/TGS_SERVER_IPS.csv";

    QString filePath = QFileDialog::getSaveFileName(this, tr("导出 CSV"), defaultFilePath, tr("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) return;

    // 启动异步导出
    m_exportFuture = ManagerSajet::exportGatewayData(filePath);

    // 使用 QFutureWatcher 监听完成
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, filePath]() {
        bool success = watcher->result();
        if (success) {
            QMessageBox::information(this, tr("成功"), tr("导出完成: %1").arg(filePath));
        } else {
            QMessageBox::critical(this, tr("错误"), tr("导出失败，请查看日志"));
        }
        OracleManager::instance().closeConnection("ExportServerIp");
        watcher->deleteLater();
    });
    watcher->setFuture(m_exportFuture);
}

