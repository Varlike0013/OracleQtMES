#include "mainwindow.h"
#include "sajetmainwindow.h"
#include "gedtmainwindow.h"
#include "oracle_manager.h"
#include "logindialog.h"  // 添加登录对话框的头文件

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QLibrary>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 加载翻译文件（保持原有逻辑不变）
    // 加载保存的语言设置
    QSettings settings;
    QString locale = settings.value("language", "zh_CN").toString();
    QTranslator translator;
    if (translator.load(QString(":/i18n/OracleQtMES_%1.qm").arg(locale))) {
        a.installTranslator(&translator);
    }

    // 2. 创建登录对话框（模态方式运行）
    LoginDialog loginDlg;
    // 如果登录失败或用户取消，直接退出程序
    if (loginDlg.exec() != QDialog::Accepted) {
        return 0;  // 用户取消了登录，直接退出
    }

    QString username = OracleManager::getCurrentUsername();
    QString dbKey = OracleManager::getCurrentDbkey();

    // 根据 dbKey 创建对应的主窗口
    QMainWindow *window = nullptr;
    if (dbKey == "SAJET") {
        window = new SajetMainWindow;
    } else if (dbKey == "GEDTA") {
        window = new GedtMainWindow;
    } else {
        window = new MainWindow;
    }
    window->show();

    return QApplication::exec();
}
