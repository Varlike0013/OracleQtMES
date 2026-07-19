#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QSqlDatabase>

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

private:
    Ui::LoginDialog *ui;
    void loadDatabaseConfigs(); // 加载数据库配置列表
};

#endif // LOGINDIALOG_H