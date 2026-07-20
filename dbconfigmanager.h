#ifndef DBCONFIGMANAGER_H
#define DBCONFIGMANAGER_H

#include <QMap>
#include <QString>
#include <QJsonObject>
#include <QWidget>

struct DbConnectionInfo
{
    QString key;
    QString host;
    int port;
    QString serviceName;
    QString username;
    QString password;
    QString note;  // 备注说明

    bool isValid() const {
        return !host.isEmpty() && !serviceName.isEmpty() && port > 0;
    }
};

class DbConfigManager
{
public:
    static DbConfigManager& instance();

    // 获取默认配置文件路径（静态方法，无需实例化）
    static QString getDefaultConfigPath();

    // 加载/保存配置
    bool loadConfig(const QString &filePath = QString());
    bool saveConfig(const QString &filePath = QString());  // 保存到文件

    // 配置管理（增删改查）
    bool hasConnection(const QString &name) const;
    DbConnectionInfo getConnection(const QString &name) const;
    void addConnection(const QString &name, const DbConnectionInfo &info);
    void removeConnection(const QString &name);
    void updateConnection(const QString &name, const DbConnectionInfo &info);
    QStringList getAllConnectionNames() const;

    // 获取配置文件路径
    QString getConfigFilePath() const { return m_configFilePath; }
    void setConfigFilePath(const QString &path) { m_configFilePath = path; }

    // 打开配置目录（方便开发者直接编辑）
    void openConfigDirectory() const;
    void openConfigFileInEditor() const;  // 用默认编辑器打开

    // 检查配置文件是否存在
    bool configFileExists() const;

private:
    DbConfigManager() = default;

    // 确保配置目录存在
    bool ensureConfigDirectoryExists() const;

    QString m_configFilePath;
    QMap<QString, DbConnectionInfo> m_configs;
    bool m_loaded = false;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &root);
};

#endif // DBCONFIGMANAGER_H