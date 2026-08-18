#ifndef CHANGEINPUT_H
#define CHANGEINPUT_H

#include <QWidget>

namespace Ui {
class ChangeInput;
}

class ChangeInput : public QWidget
{
    Q_OBJECT

public:
    explicit ChangeInput(QWidget *parent = nullptr);
    ~ChangeInput();

private slots:
    void on_pushButtonChange_clicked();

private:
    Ui::ChangeInput *ui;
    QString changeText(int typeIn,int typeOut,const QString &input);
    QStringList getListString(int typeIn, const QString &input);
    static void parseNumberedString(const QString &str, QString &prefix, int &num);
    static QStringList parseRangeString(const QString &input);// 解析范围字符串，如 "1-5" 或 "267G003884-267G003892"
};

#endif // CHANGEINPUT_H
