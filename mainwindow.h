#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dbconfigmanager.h"
#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTreeWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow;}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow( QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_searchEdit_returnPressed();
    void on_treeObjects_itemClicked(QTreeWidgetItem *item, int column);
    void on_tabWidget_tabCloseRequested(int index);
    void on_actionQuery_triggered();
    void on_actionExit_triggered();
    void on_actionSubmit_triggered();
    void on_actionRevert_triggered();

    void on_actionchangedb_triggered();

private:
    Ui::MainWindow *ui;
    QSqlDatabase m_db;              // 保存当前使用的数据库连接
    QString m_username;
    DbConnectionInfo m_connInfo;
    QString mainConnect = "main";
    QSqlTableModel *m_model = nullptr;
    QString m_currentTable;

    bool initDatabaseConnection();  // 建立数据库连接
    void setupUI();                 // 初始化界面（如状态栏显示用户信息）
    void loadDatabaseTree();        //加载数据库内容到tree
    void setStatusMessage(const QString &msg); //修改状态栏

    QTreeWidgetItem* getSchemaNode(QTreeWidgetItem *root, const QString &schema);// 获取或创建 Schema 节点（第二层）
    QTreeWidgetItem* getPrefixNode(QTreeWidgetItem *schemaItem, const QString &prefix);// 获取或创建前缀节点（第三层）
    void addDatabaseObject(QTreeWidgetItem *root,const QString &schema,const QString &fullName,
                           QMap<QString, QTreeWidgetItem*> &schemaCache,
                           QMap<QString, QTreeWidgetItem*> &prefixCache);// 通用添加对象函数（第四层叶子）
    void setItemVisibility(QTreeWidgetItem *item, const QString &searchText, bool hasSearch);// 递归设置节点可见性
    bool hasVisibleChild(QTreeWidgetItem *item);// 检查节点是否有可见的子节点

    QString getObjectType(QTreeWidgetItem *item); // 判断是表还是存储过程
    int findTabIndex(const QString &title);      // 查找已存在的 tab
    void showTableData(const QString &schema, const QString &tableName);
    void showProcedureCode(const QString &schema, const QString &procName);
    void executeQueryAndShowResult(const QString &sql, const QString &tableName, const QString &condition);
    void tabwidget_query_button();
};
#endif // MAINWINDOW_H