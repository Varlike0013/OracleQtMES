#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QSqlDatabase>
#include <QTranslator>
#include <QSettings>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    // 获取用户选择的数据库连接名（用于主窗口初始化）
    QString getSelectedConnection() const;
    QString getUsername() const;
    QString getPassword() const;

private slots:
    void on_btnConnect_clicked();
    void on_btnCancel_clicked();
    void on_radioPassword_toggled(bool checked);

    void on_comboLanguage_currentIndexChanged(int index);

private:
    Ui::LoginDialog *ui;
    void loadDatabaseConfigs(); // 加载数据库配置列表
    QString loginConnect = "logined";
    bool validateUser(QSqlDatabase &db, const QString &username, const QString &password); //数据库验证用户是否存在
    QTranslator m_translator;
    void switchLanguage(const QString &locale);   // 切换语言的核心函数
};

#endif // LOGINDIALOG_H