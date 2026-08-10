#include "managersajet.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <QCollator>
#include <algorithm>

ManagerSajet::ManagerSajet() {}


bool ManagerSajet::is_SERIAL_NUMBER(const QString &serialNumber)
{
    // 空串直接返回 false
    if (serialNumber.isEmpty()) {
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when checking SERIAL_NUMBER";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM SAJET.G_SN_STATUS WHERE SERIAL_NUMBER = :sn");
    query.bindValue(":sn", serialNumber);

    if (!query.exec()) {
        qWarning() << "Query failed in is_SERIAL_NUMBER:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}
bool ManagerSajet::is_ReworkNo(const QString &rework)
{
    // 空串直接返回 false
    if (rework.isEmpty()) {
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when checking SERIAL_NUMBER";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM SAJET.G_SN_STATUS WHERE REWORK_NO = :rework");
    query.bindValue(":rework", rework);

    if (!query.exec()) {
        qWarning() << "Query failed in is_ReworkNo:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}
bool ManagerSajet::is_CartonNo(const QString &input)
{
    // 空串直接返回 false
    if (input.isEmpty()) {
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when checking CARTON_NO";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM SAJET.G_SN_STATUS WHERE CARTON_NO = :input");
    query.bindValue(":input", input);

    if (!query.exec()) {
        qWarning() << "Query failed in is_CartonNo:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}
bool ManagerSajet::is_WorkOrderNo(const QString &input)
{
    // 空串直接返回 false
    if (input.isEmpty()) {
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when checking WORK_ORDER";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM SAJET.G_SN_STATUS WHERE WORK_ORDER = :input");
    query.bindValue(":input", input);

    if (!query.exec()) {
        qWarning() << "Query failed in is_WorkOrderNo:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}
bool ManagerSajet::is_QcNo(const QString &input)
{
    // 空串直接返回 false
    if (input.isEmpty()) {
        return false;
    }

    QSqlDatabase db = OracleManager::instance().getCurrentDbMain();
    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "Database connection invalid when checking QC_NO";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM SAJET.G_SN_STATUS WHERE QC_NO = :input");
    query.bindValue(":input", input);

    if (!query.exec()) {
        qWarning() << "Query failed in is_QcNo:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}
static QString getGatewayIP(QSqlDatabase &db, const QString &serverId,
                            const QString &gatewayId, const QString &driverId,
                            QString &error)
{
    QSqlQuery query(db);
    query.prepare("BEGIN SAJET.GET_GATEWAY_IP(:server, :gateway, :driver, :tres, :ip); END;");
    query.bindValue(":server", serverId);
    query.bindValue(":gateway", gatewayId);
    query.bindValue(":driver", driverId);
    QString tres, ipStr;
    tres.reserve(100);
    ipStr.reserve(1000);
    query.bindValue(":tres", tres, QSql::Out);
    query.bindValue(":ip", ipStr, QSql::Out);

    if (!query.exec()) {
        error = query.lastError().text();
        return QString();
    }
    tres = query.boundValue(":tres").toString();
    ipStr = query.boundValue(":ip").toString();
    if (tres != "OK") {
        error = tres;
        return QString();
    }
    return ipStr;
}

static QStringList extractIPs(const QString &ipStr)
{
    QStringList ips;
    QRegularExpression re(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
    QRegularExpressionMatchIterator it = re.globalMatch(ipStr);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        ips << match.captured(0);
    }
    return ips;
}

static void queryTerminalStatus(QSqlDatabase &db, const QString &serverId,
                                const QString &gatewayId, int idx,
                                const QString &gatewayDesc,
                                QList<QStringList> &rows)
{
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
        "WHERE TL.SERVER_ID = :server AND TL.GATEWAY_ID = :gateway AND TL.DEVICE_ID = :idx";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":server", serverId);
    query.bindValue(":gateway", gatewayId);
    query.bindValue(":idx", idx);

    if (!query.exec()) {
        qWarning() << "Terminal status query failed:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QStringList row;
        row << query.value(0).toString()   // STAGE_NAME
            << query.value(1).toString()   // PDLINE_NAME
            << gatewayDesc                 // GATEWAY_DESC_E
            << query.value(3).toString()   // TERMINAL_ID
            << query.value(4).toString()   // TERMINAL_NAME
            << query.value(5).toString()   // PROCESS_NAME
            << query.value(6).toString();  // GROUP_ID
        rows.append(row);
    }
}

bool ManagerSajet::exportGatewayDataSync(const QString &filePath, QString &errorMsg)
{
    // 1. 获取连接信息并打开临时连接
    OracleManager& oracleMgr = OracleManager::instance();
    DbConnectionInfo connInfo = oracleMgr.getCurrentDbInfo(); // 假设此方法返回当前连接信息
    DbConnectionResult result = oracleMgr.connectDatabase(connInfo, "ExportServerIp");

    if (!result.success) {
        errorMsg = QString("Failed to connect to database : %1").arg(result.errorMessage);
        return false;
    }
    QSqlDatabase db = result.database;
    if (!db.isOpen()) {
        errorMsg = "Database connection is not open.";
        oracleMgr.closeConnection("ExportServerIp");
        return false;
    }
    // 2. 查询所有服务器和网关
    QString sqlGateway =
        "SELECT S.SERVER_ID, S.SERVER_DESC_E, G.GATEWAY_ID, G.DRIVER_ID, "
        "G.GAYTEWAY_CONNECT_NUMBER, G.GATEWAY_DESC_E "
        "FROM SAJET.TGS_SERVER_BASE S "
        "INNER JOIN SAJET.TGS_GATEWAY_BASE G ON G.SERVER_ID = S.SERVER_ID "
        "WHERE S.ENABLED = 'Y' AND G.ENABLED = 'Y' "
        "ORDER BY S.SERVER_DESC_E";
    QSqlQuery query(db);
    if (!query.exec(sqlGateway)) {
        errorMsg = query.lastError().text();
        db.close();
        QSqlDatabase::removeDatabase("ExportServerIp");
        return false;
    }

    // 3. 准备 CSV 数据
    QList<QStringList> csvRows;
    QStringList header;
    header << "IP网段" << "完整IP" << "Server ID" << "Gateway ID" << "Driver ID"
           << "站别" << "产线" << "网关描述" << "终端ID" << "终端名称"
           << "工序" << "组ID";
    csvRows.append(header);

    while (query.next()) {
        QString serverId = query.value("SERVER_ID").toString();
        QString gatewayId = query.value("GATEWAY_ID").toString();
        QString driverId = query.value("DRIVER_ID").toString();
        QString gatewayDesc = query.value("GATEWAY_DESC_E").toString();

        QString error;
        QString ipStr = getGatewayIP(db, serverId, gatewayId, driverId, error);
        if (ipStr.isEmpty()) {
            qWarning() << "Skipping gateway due to error:" << error;
            continue;
        }

        QStringList ips = extractIPs(ipStr);
        if (ips.isEmpty()) continue;

        int idx = 0;
        for (const QString &ip : ips) {
            QString network = ip;
            int lastDot = ip.lastIndexOf('.');
            if (lastDot != -1) network = ip.left(lastDot);

            QList<QStringList> terminalRows;
            queryTerminalStatus(db, serverId, gatewayId, idx, gatewayDesc, terminalRows);

            if (terminalRows.isEmpty()) {
                QStringList row;
                row << network << ip << serverId << gatewayId << driverId
                    << "" << "" << gatewayDesc << "" << "" << "" << "";
                csvRows.append(row);
            } else {
                for (const QStringList &termRow : terminalRows) {
                    QStringList row;
                    row << network << ip << serverId << gatewayId << driverId
                        << termRow[0] << termRow[1] << termRow[2]
                        << termRow[3] << termRow[4] << termRow[5] << termRow[6];
                    csvRows.append(row);
                }
            }
            idx++;
        }
    }

    db.close();

    // 4. 排序（按网段和完整 IP）
    if (csvRows.size() > 1) {
        QCollator collator;
        collator.setNumericMode(true);
        QList<QStringList> dataRows = csvRows.mid(1);
        std::sort(dataRows.begin(), dataRows.end(),
                  [&](const QStringList &a, const QStringList &b) {
                      int cmp = collator.compare(a[0], b[0]);
                      if (cmp != 0) return cmp < 0;
                      return collator.compare(a[1], b[1]) < 0;
                  });
        csvRows.clear();
        csvRows.append(header);
        csvRows.append(dataRows);
    }

    // 5. 写入 CSV 文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QString("Canot created file: %1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    for (const QStringList &row : csvRows) {
        QStringList quoted;
        for (const QString &field : row) {
            QString f = field;
            if (f.contains(',') || f.contains('"') || f.contains('\n')) {
                f.replace("\"", "\"\"");
                f = "\"" + f + "\"";
            }
            quoted << f;
        }
        out << quoted.join(",") << "\n";
    }
    file.close();
    return true;
}

QFuture<bool> ManagerSajet::exportGatewayData(const QString &filePath)
{
    // 使用 QtConcurrent 异步执行同步导出函数
    return QtConcurrent::run([filePath]() -> bool {
        QString error;
        bool result = exportGatewayDataSync(filePath, error);
        if (!result) {
            // 可记录错误，但 QFuture 本身不直接传递错误信息，可通过 QFutureWatcher 的 result() 获取
            // 或使用 QFuture::exception 等，但这里简单返回 false
            qWarning() << "Export failed:" << error;
        }
        return result;
    });
}
QString ManagerSajet::updateErpDataSync()
{
    // 1. 获取当前连接信息
    OracleManager& oracleMgr = OracleManager::instance();
    DbConnectionInfo connInfo = oracleMgr.getCurrentDbInfo();
    DbConnectionResult result = oracleMgr.connectDatabase(connInfo, "UpdateAsusErp");
    if (!connInfo.isValid()) {
        return "Current database connection info is invalid.";
    }
    if (!result.success) {
        return QString("Failed to connect to database : %1").arg(result.errorMessage);
    }
    // 2. 打开对应连接
    QSqlDatabase db = result.database;
    if (!db.open()) {
        return QString("Failed to open database connection: %1").arg(db.lastError().text());
        oracleMgr.closeConnection("UpdateAsusErp");
    }

    // 3. 执行存储过程（无参数）
    QSqlQuery query(db);
    if (!query.exec("BEGIN SAJET.erp_to_sfis_asus; END;")) {
        QString err = query.lastError().text();
        db.close();
        oracleMgr.closeConnection("UpdateAsusErp");
        return err;
    }

    // 4. 提交事务
    if (!db.commit()) {
        QString err = db.lastError().text();
        db.close();
        oracleMgr.closeConnection("UpdateAsusErp");
        return err;
    }

    db.close();
    //oracleMgr.closeConnection("UpdateAsusErp");
    return "OK";
}

QFuture<QString> ManagerSajet::updateErpData()
{
    return QtConcurrent::run(updateErpDataSync);
}