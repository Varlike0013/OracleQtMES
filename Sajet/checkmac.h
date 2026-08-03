#ifndef CHECKMAC_H
#define CHECKMAC_H

#include <QWidget>

namespace Ui {
class CheckMac;
}

class CheckMac : public QWidget
{
    Q_OBJECT

public:
    explicit CheckMac(QWidget *parent = nullptr);
    ~CheckMac();

    // 全局条件字典管理
    static void addCondition(const QString &key, const QString &value);
    static bool removeCondition(const QString &key, const QString &value);
    static void clearConditions();
    static QStringList getConditionValues(const QString &key);
    static QMap<QString, QStringList> getAllConditions();
    static bool conditionExists(const QString &key, const QString &value);
    void UpadteTableMacs();

private slots:
    void on_lineEditInput_returnPressed();
    void on_pushButton_clicked();

    void on_pushButtonDelete_clicked();

private:
    Ui::CheckMac *ui;
    static QMap<QString, QStringList> m_conditions;   // 全局条件字典
};

#endif // CHECKMAC_H
