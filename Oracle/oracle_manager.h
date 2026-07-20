#ifndef ORACLE_MANAGER_H
#define ORACLE_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
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
    static const CurrentConnectInfo& getCurrentConnectInfo();
    static bool isUserLoggedIn();
    static QString getCurrentUsername();
    static QString getCurrentDbname();
    static DbConnectionInfo getCurrentConnection();


    DbConnectionResult connectDatabase(const DbConnectionInfo &connInfo,const QString &connectname); // 尝试连接数据库（根据连接信息）

    void closeConnection(const QString &connectname);
    void setCurrentUser(const QString &username, const QString &password, const DbConnectionInfo &info);

    QStringList getOpenConnectionNames() const; // 获取所有已打开的连接名称列表

private:
    explicit OracleManager(QObject *parent = nullptr);
    ~OracleManager();

    static OracleManager* m_instance;
    static CurrentConnectInfo m_currentConnectInfo;//记录当前登陆用户信息和数据库链接
    QStringList m_openConnections; // 用于保存所有已打开连接名称的列表
};

#endif // ORACLE_MANAGER_H
