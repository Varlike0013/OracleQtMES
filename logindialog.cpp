#include "logindialog.h"
#include "ui_logindialog.h"
#include "dbconfigmanager.h"
#include "oracle_manager.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSettings>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle("数据库登录");
    loadDatabaseConfigs();

    // 读取保存的语言设置
    QSettings settings;
    QString defaultLocale = settings.value("language", "zh_CN").toString();
    // 设置下拉框的当前索引
    int index = ui->comboLanguage->findData(defaultLocale);
    if (index == -1) {
        index = ui->comboLanguage->findData("zh_CN"); // 如果找不到，默认中文
    }
    ui->comboLanguage->setCurrentIndex(index);
    switchLanguage(defaultLocale); // 应用语言（注意：此时窗口尚未显示，但为了让翻译生效，先加载一次）
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
    // 3.获取数据库信息 使用 OracleManager 测试数据库连通性
    DbConnectionInfo connInfo = configMgr.getConnection(dbName);
    OracleManager& oracleMgr = OracleManager::instance();
    DbConnectionResult result = oracleMgr.connectDatabase(connInfo,loginConnect);

    if (!result.success) {
        QMessageBox::critical(this, "连接失败",
                              "无法连接到数据库:\n" + result.errorMessage);
        return;
    }
    // 4. 连接成功，调用存储过程验证用户
    if (!validateUser(result.database, user, pass)) {
        // 验证失败，关闭连接并返回
        oracleMgr.closeConnection(loginConnect);
        return;
    }
    // 5. 连接成功，保存连接信息
    OracleManager& mgr = OracleManager::instance();
    mgr.setCurrentUser(user, pass, connInfo);
    // 6. 关闭测试连接（释放资源）
    oracleMgr.closeConnection(loginConnect);

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
void LoginDialog::on_btnCancel_clicked()
{
    reject();
}
bool LoginDialog::validateUser(QSqlDatabase &db, const QString &username, const QString &password)
{
    QSqlQuery query(db);
    QString resultStr;
    resultStr.reserve(200);
    // 调用存储过程 check_emp
    query.prepare("BEGIN SAJET.SJ_CHK_EMP_PWD(:username, :password, :result); END;");
    query.bindValue(":username", username);
    query.bindValue(":password", password);
    query.bindValue(":result", resultStr,QSql::Out);

    if (!query.exec()) {
        QMessageBox::critical(this, "存储过程调用失败",
                              "执行 SJ_CHK_EMP_PWD 存储过程出错:\n" + query.lastError().text());
        return false;
    }
    QString result = query.boundValue(":result").toString();
    if (result == "OK") {
        return true;
    } else {
        QMessageBox::warning(this, "登录失败", "验证失败：" + result);
        return false;
    }
}
void LoginDialog::on_radioPassword_toggled(bool checked)
{
    if (checked) {
        ui->editPassword->setEchoMode(QLineEdit::Normal);
    } else {
        ui->editPassword->setEchoMode(QLineEdit::Password);
    }
}
void LoginDialog::on_comboLanguage_currentIndexChanged(int index)
{
    if (index < 0) return;
    QString locale = ui->comboLanguage->currentText();
    if (locale == "中文"){locale = "zh_CN";}
    else if (locale == "English"){locale = "en";}
    else {locale = "zh_CN";}
    if (locale.isEmpty()) return;
    switchLanguage(locale);
}
void LoginDialog::switchLanguage(const QString &locale)
{
    qApp->removeTranslator(&m_translator);
    QString qmFile = QString(":/i18n/OracleQtMES_%1.qm").arg(locale);
    if (m_translator.load(qmFile)) {
        qApp->installTranslator(&m_translator);
    } else {
        if (m_translator.load(QString("translations/OracleQtMES_%1.qm").arg(locale))) {
            qApp->installTranslator(&m_translator);
        }
    }
    ui->retranslateUi(this);
    QSettings settings;
    settings.setValue("language", locale);
}

