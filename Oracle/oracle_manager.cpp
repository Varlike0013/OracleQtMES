#include "oracle_manager.h"
#include <QDebug>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>

// 静态成员初始化
OracleManager* OracleManager::m_instance = nullptr;
CurrentConnectInfo OracleManager::m_currentConnectInfo;

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
    for (const QString &name : m_openConnections) {
        closeConnection(name);
    }
    m_instance = nullptr;
}

DbConnectionResult OracleManager::connectDatabase(const DbConnectionInfo &connInfo, const QString &connectname)
{
    DbConnectionResult result;
    result.success = false;

    // 1. 如果已存在同名连接，先关闭并移除
    closeConnection(connectname);

    // 2. 创建数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QOCI", connectname);
    db.setHostName(connInfo.host);
    db.setPort(connInfo.port);
    db.setDatabaseName(connInfo.serviceName);
    db.setUserName(connInfo.username);
    db.setPassword(connInfo.password);

    // 3. 尝试连接
    if (!db.open()) {
        result.errorMessage = db.lastError().text();
        qWarning() << "数据库连接失败:" << result.errorMessage;
        // 确保关闭并移除
        if (db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(connectname);
        return result;
    }

    // 4. 连接成功
    result.success = true;
    result.database = db;

    // 将连接名称添加到列表
    if (!m_openConnections.contains(connectname)) {
        m_openConnections.append(connectname);
    }
    qDebug() << "数据库连接成功! 主机:" << connInfo.host << "服务名:" << connInfo.serviceName;
    return result;
}

void OracleManager::closeConnection(const QString &connectname)
{
    // 关闭并移除测试连接
    if (QSqlDatabase::contains(connectname)) {
        QSqlDatabase db = QSqlDatabase::database(connectname);
        if (db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(connectname);
        m_openConnections.removeOne(connectname);
        qDebug() <<connectname<< "连接已关闭";
    }
}
void OracleManager::setCurrentUser(const QString &username, const QString &password, const DbConnectionInfo &info)
{
    m_currentConnectInfo.isLoggedIn = true;
    m_currentConnectInfo.username = username;
    m_currentConnectInfo.password = password;
    m_currentConnectInfo.connectionInfo = info;
    m_currentConnectInfo.loginTime = QDateTime::currentDateTime();
}
const CurrentConnectInfo& OracleManager::getCurrentConnectInfo()
{
    return m_currentConnectInfo;
}
bool OracleManager::isUserLoggedIn()
{
    return m_currentConnectInfo.isLoggedIn;
}
QString OracleManager::getCurrentUsername()
{
    return m_currentConnectInfo.username;
}
DbConnectionInfo OracleManager::getCurrentConnection()
{
    return m_currentConnectInfo.connectionInfo;
}
QStringList OracleManager::getOpenConnectionNames() const
{
    return m_openConnections;
}
QString OracleManager::getCurrentDbname()
{
    return m_currentConnectInfo.connectionInfo.serviceName;
}
QString OracleManager::getCurrentDbkey()
{
    return m_currentConnectInfo.connectionInfo.key;
}
