#include "dbconfigmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>


DbConfigManager& DbConfigManager::instance()
{
    static DbConfigManager instance;
    return instance;
}
// 静态方法：获取默认配置文件路径（程序目录下的 config 文件夹）
QString DbConfigManager::getDefaultConfigPath()
{
    // 获取应用程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();

    // 在程序目录下创建 config 文件夹
    QString configDir = appDir + "/config";

    // 确保目录存在
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return configDir + "/db_config.json";
}

// 确保配置目录存在
bool DbConfigManager::ensureConfigDirectoryExists() const
{
    if (m_configFilePath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(m_configFilePath);
    QDir dir = fileInfo.absoluteDir();

    if (!dir.exists()) {
        return dir.mkpath(".");
    }

    return true;
}

bool DbConfigManager::loadConfig(const QString &filePath)
{
    // 如果未指定文件路径，使用默认路径
    QString path = filePath;
    if (path.isEmpty()) {
        path = getDefaultConfigPath();
    }

    m_configFilePath = path;

    // 确保配置目录存在
    if (!ensureConfigDirectoryExists()) {
        qWarning() << "无法创建配置目录";
        return false;
    }

    // 如果配置文件不存在，创建一个默认的配置文件
    if (!QFile::exists(path)) {
        qWarning() << "配置文件不存在，正在创建默认配置...";

        // ----- 创建默认配置（使用新的 JSON 结构）-----
        DbConnectionInfo defaultSajet;
        defaultSajet.key = "SAJET";
        defaultSajet.host = "10.240.144.17";
        defaultSajet.port = 1521;
        defaultSajet.serviceName = "SAJET";
        defaultSajet.username = "SAJET";
        defaultSajet.password = "tech";
        defaultSajet.note = "GESZ";
        addConnection("SAJET", defaultSajet);

        DbConnectionInfo defaultGedt;
        defaultGedt.key = "GEDTA";
        defaultGedt.host = "10.240.144.180";
        defaultGedt.port = 1521;
        defaultGedt.serviceName = "SAJET";
        defaultGedt.username = "SAJET";
        defaultGedt.password = "tech";
        defaultGedt.note = "GESZ";
        addConnection("GEDTA", defaultGedt);

        // 保存默认配置到文件
        if (saveConfig()) {
            qDebug() << "已创建默认配置文件:" << path;
        } else {
            qWarning() << "创建默认配置文件失败";
        }
    }

    // 打开并读取配置文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开配置文件:" << path;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "无效的 JSON 格式";
        return false;
    }

    m_loaded = fromJson(doc.object());
    return m_loaded;
}

bool DbConfigManager::saveConfig(const QString &filePath)
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = m_configFilePath;
    }

    if (path.isEmpty()) {
        // 如果还没有设置路径，使用默认路径
        path = getDefaultConfigPath();
        m_configFilePath = path;
    }

    // 确保配置目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Can't create config directory:" << dir.path();
            return false;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Can't write to the config file:" << path;
        return false;
    }

    QJsonObject root = toJson();
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));  // 格式化输出，便于阅读
    file.close();

    qDebug() << "The configuration file has been saved:" << path;
    return true;
}

QJsonObject DbConfigManager::toJson() const
{
    QJsonObject root;
    root["_comment"] = "数据库连接配置文件 - 可直接用文本编辑器修改";
    root["_instructions"] = "每个连接需包含: host(主机), port(端口), service_name(服务名), username(用户名), password(密码)";
    root["_config_location"] = "配置文件位于程序目录下的 config 文件夹中";

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        QJsonObject connObj;
        const DbConnectionInfo &info = it.value();
        connObj["host"] = info.host;
        connObj["port"] = info.port;
        connObj["service_name"] = info.serviceName;
        connObj["username"] = info.username;
        connObj["password"] = info.password;
        if (!info.note.isEmpty()) {
            connObj["_note"] = info.note;
        }
        root[it.key()] = connObj;
    }
    return root;
}

bool DbConfigManager::fromJson(const QJsonObject &root)
{
    m_configs.clear();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QString key = it.key();
        if (key.startsWith("_")) continue;

        QJsonObject connObj = it.value().toObject();
        DbConnectionInfo info;
        info.key = key;   // 保存键名
        info.host = connObj["host"].toString();
        info.port = connObj["port"].toInt(1521);
        info.serviceName = connObj["service_name"].toString();
        info.username = connObj["username"].toString();
        info.password = connObj["password"].toString();
        info.note = connObj["_note"].toString();

        if (info.isValid()) {
            m_configs[key] = info;
        } else {
            qWarning() << "Skip invalid configuration:" << key;
        }
    }
    m_loaded = true;
    return true;
}

void DbConfigManager::openConfigDirectory() const
{
    if (m_configFilePath.isEmpty()) return;
    QFileInfo fileInfo(m_configFilePath);
    QString dirPath = fileInfo.absoluteDir().path();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
}

void DbConfigManager::openConfigFileInEditor() const
{
    if (m_configFilePath.isEmpty() || !QFile::exists(m_configFilePath)) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_configFilePath));
}

bool DbConfigManager::configFileExists() const
{
    QString path = m_configFilePath.isEmpty() ? getDefaultConfigPath() : m_configFilePath;
    return QFile::exists(path);
}

void DbConfigManager::addConnection(const QString &name, const DbConnectionInfo &info)
{
    m_configs[name] = info;
}

void DbConfigManager::removeConnection(const QString &name)
{
    m_configs.remove(name);
}

void DbConfigManager::updateConnection(const QString &name, const DbConnectionInfo &info)
{
    if (m_configs.contains(name)) {
        m_configs[name] = info;
    }
}

bool DbConfigManager::hasConnection(const QString &name) const
{
    return m_configs.contains(name);
}

DbConnectionInfo DbConfigManager::getConnection(const QString &name) const
{
    return m_configs.value(name, DbConnectionInfo());
}

QStringList DbConfigManager::getAllConnectionNames() const
{
    return m_configs.keys();
}