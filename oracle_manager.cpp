#include "oracle_manager.h"
#include <QDebug>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QFile>
#include <QFileDialog>
#include <QTreeWidgetItem>

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
    qDebug() << "OracleManager Initialize";
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
        qWarning() << "Failed to connect to the database:" << result.errorMessage;
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
    qDebug() << "Database connection successful! Host:" << connInfo.host << "Service Name:" << connInfo.serviceName;
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
        qDebug() <<connectname<< "Connection closed";
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
const CurrentConnectInfo& OracleManager::getCurrentConnect()
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
DbConnectionInfo OracleManager::getCurrentDbInfo()
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
void OracleManager::setCurrentDbMain(const QString &connName) {
    m_connectionName = connName;
}
QSqlDatabase OracleManager::getCurrentDbMain() const {
    // 总是从 Qt 的全局连接池中按名称获取
    if (m_connectionName.isEmpty()) {
        return QSqlDatabase(); // 返回无效连接
    }
    return QSqlDatabase::database(m_connectionName, false); // false 表示不报错
}
bool OracleManager::isDatabaseValid() const {
    QSqlDatabase db = getCurrentDbMain();
    return db.isValid() && db.isOpen();
}
void OracleManager::disconnectAll()
{
    // 复制一份连接名称列表，因为 closeConnection 会修改 m_openConnections
    QStringList connections = m_openConnections;
    for (const QString &name : connections) {
        closeConnection(name);
    }
    // 确保列表清空（closeConnection 应该已经移除所有，但以防万一）
    m_openConnections.clear();
    qDebug() << "All database connections have been closed";
}
QString OracleManager::exportTableViewToCsv(QTableView *tableView, QWidget *parent)
{
    if (!tableView) {
        return "exportTableViewToCsv: tableView is null";
    }

    QAbstractItemModel *model = tableView->model();
    if (!model) {
        return "exportTableViewToCsv: model is null";
    }

    // 让用户选择保存路径
    QString defaultFileName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";
    QString filePath = QFileDialog::getSaveFileName(parent,
                                                    QObject::tr("导出 CSV"),
                                                    defaultFileName,
                                                    QObject::tr("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) {
        return "user canle";
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString("can't created file: %1").arg(filePath);
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out.setGenerateByteOrderMark(true);

    int rowCount = model->rowCount();
    int colCount = model->columnCount();

    // 写表头
    QStringList headerItems;
    for (int col = 0; col < colCount; ++col) {
        QString header = model->headerData(col, Qt::Horizontal).toString();
        if (header.isEmpty()) {
            header = QString("Column%1").arg(col + 1);
        }
        if (header.contains(',') || header.contains('"') || header.contains('\n')) {
            header.replace("\"", "\"\"");
            header = "\"" + header + "\"";
        }
        headerItems << header;
    }
    out << headerItems.join(",") << "\n";

    // 写数据行
    for (int row = 0; row < rowCount; ++row) {
        QStringList rowItems;
        for (int col = 0; col < colCount; ++col) {
            QModelIndex index = model->index(row, col);
            QString data = model->data(index).toString();
            if (data.contains(',') || data.contains('"') || data.contains('\n')) {
                data.replace("\"", "\"\"");
                data = "\"" + data + "\"";
            }
            rowItems << data;
        }
        out << rowItems.join(",") << "\n";
    }

    file.close();
    return QString();   // 成功返回空字符串
}
void OracleManager::setItemVisibility(QTreeWidgetItem *item, const QString &searchText, bool hasSearch)
{
    if (!item) return;

    bool isLeaf = (item->childCount() == 0);

    if (isLeaf) {
        // 叶子节点：匹配全名（不区分大小写）
        if (hasSearch) {
            QString name = item->text(0);
            bool match = name.contains(searchText, Qt::CaseInsensitive);
            item->setHidden(!match);
        } else {
            item->setHidden(false); // 无搜索时全部显示
        }
    } else {
        // 非叶子节点：先递归处理所有子节点
        for (int i = 0; i < item->childCount(); ++i) {
            setItemVisibility(item->child(i), searchText, hasSearch);
        }
        // 根据是否有可见子节点决定自身是否隐藏
        bool hasVisible = hasVisibleChild(item);
        item->setHidden(!hasVisible);

        // 如果处于搜索模式且自身可见，则展开（显示路径）
        if (hasSearch) {
            item->setExpanded(hasVisible);
        } else {
            item->setExpanded(true);
        }
    }
}
bool OracleManager::hasVisibleChild(QTreeWidgetItem *item)
{
    if (!item) return false;
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (!child->isHidden()) {
            return true;
        }
        if (hasVisibleChild(child)) {
            return true;
        }
    }
    return false;
}
