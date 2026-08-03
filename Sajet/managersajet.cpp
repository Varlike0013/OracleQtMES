#include "managersajet.h"
#include <qsqlquery.h>

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