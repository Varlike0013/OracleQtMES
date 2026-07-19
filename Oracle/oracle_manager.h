#ifndef ORACLE_MANAGER_H
#define ORACLE_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include "dbconfigmanager.h"

// 数据库连接结果
struct DbConnectionResult
{
    bool success = false;
    QString errorMessage;
    QSqlDatabase database;
};

class OracleManager : public QObject
{
    Q_OBJECT

public:
    static OracleManager& instance();

    // 尝试连接数据库（根据连接信息）
    DbConnectionResult connectDatabase(const DbConnectionInfo &connInfo);

    // 关闭测试连接
    void closeTestConnection();

private:
    explicit OracleManager(QObject *parent = nullptr);
    ~OracleManager();

    static OracleManager* m_instance;

    QString m_testConnectionName = "test_conn";
};

#endif // ORACLE_MANAGER_H
