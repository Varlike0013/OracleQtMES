#include "logindialog.h"
#include "ui_logindialog.h"
#include "dbconfigmanager.h"
#include "oracle_manager.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle("数据库登录");
    loadDatabaseConfigs();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::loadDatabaseConfigs()
{
    ui->comboDatabase->clear();
    DbConfigManager& configMgr = DbConfigManager::instance();

    if (!configMgr.loadConfig()) {
        qWarning() << "加载配置文件失败";
        QMessageBox::warning(this, "警告", "加载数据库配置失败，请检查配置");
        return;
    }

    // 获取所有连接名称，填充下拉框
    const QStringList connNames = configMgr.getAllConnectionNames();
    if (connNames.isEmpty()) {
        qWarning() << "配置文件中没有有效的数据库连接";
        QMessageBox::warning(this, "警告", "没有可用的数据库配置");
        return;
    }

    // 将连接添加到下拉框
    for (const QString &name : connNames) {
        DbConnectionInfo info = configMgr.getConnection(name);
        QString displayName = name;
        if (!info.note.isEmpty()) {
            displayName = name + " (" + info.note + ")";
        }
        ui->comboDatabase->addItem(displayName, name);
    }

    // 默认选中第一个
    if (ui->comboDatabase->count() > 0) {
        ui->comboDatabase->setCurrentIndex(0);
    }
}

void LoginDialog::on_btnConnect_clicked()
{
    // 1. 获取用户输入
    QString dbName = ui->comboDatabase->currentData().toString();
    QString user = ui->editUsername->text().trimmed();
    QString pass = ui->editPassword->text().trimmed();

    if (user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "用户名和密码不能为空");
        return;
    }

    // 2. 获取数据库连接配置（从 DbConfigManager）
    DbConfigManager& configMgr = DbConfigManager::instance();
    if (!configMgr.hasConnection(dbName)) {
        QMessageBox::warning(this, "错误", "未找到数据库配置: " + dbName);
        return;
    }
    // 3.获取数据库信息
    DbConnectionInfo connInfo = configMgr.getConnection(dbName);
    // 4. 使用 OracleManager 测试数据库连通性
    OracleManager& oracleMgr = OracleManager::instance();
    DbConnectionResult result = oracleMgr.connectDatabase(connInfo);

    if (!result.success) {
        QMessageBox::critical(this, "连接失败",
                              "无法连接到数据库:\n" + result.errorMessage);
        return;
    }

    // 5. 连接成功，保存连接信息

    // 6. 关闭测试连接（释放资源）
    oracleMgr.closeTestConnection();

    QMessageBox::information(this, "成功", "数据库连接成功！");
    accept();  // 关闭对话框，返回 QDialog::Accepted
}

QString LoginDialog::getSelectedConnection() const
{
    return ui->comboDatabase->currentData().toString();
}

QString LoginDialog::getUsername() const
{
    return ui->editUsername->text().trimmed();
}

QString LoginDialog::getPassword() const
{
    return ui->editPassword->text().trimmed();
}