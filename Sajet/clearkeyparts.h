#ifndef CLEARKEYPARTS_H
#define CLEARKEYPARTS_H

#include <QWidget>

namespace Ui {
class ClearKeyparts;
}

class ClearKeyparts : public QWidget
{
    Q_OBJECT

public:
    explicit ClearKeyparts(QWidget *parent = nullptr);
    ~ClearKeyparts();
    // 全局条件字典管理
    static void addCondition(const QString &key, const QString &value);
    static bool removeCondition(const QString &key, const QString &value);
    static void clearConditions();
    static QStringList getConditionValues(const QString &key);
    static QMap<QString, QStringList> getAllConditions();
    static bool conditionExists(const QString &key, const QString &value);
    void UpadteTableRow();

private slots:
    void on_lineEditInput_returnPressed();
    void on_pushButtonSelect_clicked();

    void on_pushButtonClear_clicked();

private:
    Ui::ClearKeyparts *ui;
    static QMap<QString, QStringList> m_conditions;   // 全局条件字典
};

#endif // CLEARKEYPARTS_H
