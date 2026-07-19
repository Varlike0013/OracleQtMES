#include "oracle_manager.h"
#include <QDebug>

// 静态成员初始化
OracleManager* OracleManager::m_instance = nullptr;

OracleManager& OracleManager::instance()
{
    if (!m_instance) {
        m_instance = new OracleManager();
    }
    return *m_instance;
}

OracleManager::OracleManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "OracleManager 初始化";
}

OracleManager::~OracleManager()
{
    closeTestConnection();
    m_instance = nullptr;
}

DbConnectionResult OracleManager::connectDatabase(const DbConnectionInfo &connInfo)
{
    DbConnectionResult result;
    result.success = false;

    // 1. 先关闭之前的测试连接（如果有）
    closeTestConnection();

    // 2. 创建数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QOCI", m_testConnectionName);
    db.setHostName(connInfo.host);
    db.setPort(connInfo.port);
    db.setDatabaseName(connInfo.serviceName);
    db.setUserName(connInfo.username);
    db.setPassword(connInfo.password);

    // 3. 尝试连接
    if (!db.open()) {
        result.errorMessage = db.lastError().text();
        qWarning() << "数据库连接失败:" << result.errorMessage;
        QSqlDatabase::removeDatabase(m_testConnectionName);
        return result;
    }

    // 4. 连接成功
    result.success = true;
    result.database = db;
    qDebug() << "数据库连接成功! 主机:" << connInfo.host << "服务名:" << connInfo.serviceName;

    return result;
}

void OracleManager::closeTestConnection()
{
    // 关闭并移除测试连接
    if (QSqlDatabase::contains(m_testConnectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_testConnectionName);
        if (db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(m_testConnectionName);
        qDebug() << "测试连接已关闭";
    }
}