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
#include <windows.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 检查命令行参数是否包含 "console"（不区分大小写）
    QStringList args = QCoreApplication::arguments();
    bool showConsole = args.contains("console", Qt::CaseInsensitive) ||
                       args.contains("--console", Qt::CaseInsensitive) ||
                       args.contains("/console", Qt::CaseInsensitive);

    if (showConsole) {
        // 尝试附加到父进程的控制台（例如从 PowerShell 或 CMD 启动）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            // 如果失败（例如从资源管理器双击启动），则分配新控制台
            AllocConsole();
        }
        // 重定向标准输出流到控制台
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        // 可选：设置 UTF-8 编码以正确显示中文
        SetConsoleOutputCP(CP_UTF8);
    }


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
