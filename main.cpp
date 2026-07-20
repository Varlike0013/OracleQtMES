#include "mainwindow.h"
#include "logindialog.h"  // 添加登录对话框的头文件
#include "oracle_manager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QLibrary>



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 加载翻译文件（保持原有逻辑不变）
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "OracleQtMES_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // 2. 创建登录对话框（模态方式运行）
    LoginDialog loginDlg;
    // 如果登录失败或用户取消，直接退出程序
    if (loginDlg.exec() != QDialog::Accepted) {
        return 0;  // 用户取消了登录，直接退出
    }

    Db connect = OracleManager::getCurrentConnection();
    QString username = user.username;
    QString dbName = user.connectionName;

    MainWindow w;
    w.show();
    return QApplication::exec();
}
