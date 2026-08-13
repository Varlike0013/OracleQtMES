#ifndef ORACLE_MANAGER_H
#define ORACLE_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include <QSqlQuery>
#include "dbconfigmanager.h"

struct DbConnectionResult // 数据库连接结果
{
    bool success = false;
    QString errorMessage;
    QSqlDatabase database;
};
struct CurrentConnectInfo //数据库链接信息
{
    bool isLoggedIn = false;
    QString username;
    QString password;
    DbConnectionInfo connectionInfo;
    QDateTime loginTime;
};
class OracleManager : public QObject
{
    Q_OBJECT

public:
    static OracleManager& instance();
    static const CurrentConnectInfo& getCurrentConnect();
    static bool isUserLoggedIn();
    static QString getCurrentUsername();
    static QString getCurrentDbname();
    static QString getCurrentDbkey();
    static DbConnectionInfo getCurrentDbInfo();

    DbConnectionResult connectDatabase(const DbConnectionInfo &connInfo,const QString &connectname); // 尝试连接数据库（根据连接信息）
    void closeConnection(const QString &connectname);
    void setCurrentUser(const QString &username, const QString &password, const DbConnectionInfo &info);
    QStringList getOpenConnectionNames() const; // 获取所有已打开的连接名称列表
    void disconnectAll();
    void setCurrentDbMain(const QString &connName);
    QSqlDatabase getCurrentDbMain() const; // 返回副本，但总是从全局池取
    bool isDatabaseValid() const;

private:
    explicit OracleManager(QObject *parent = nullptr);
    ~OracleManager();

    static OracleManager* m_instance;
    static CurrentConnectInfo m_currentConnectInfo;//记录当前登陆用户信息和数据库链接
    QString m_connectionName;
    QStringList m_openConnections; // 用于保存所有已打开连接名称的列表
};

#endif // ORACLE_MANAGER_H
