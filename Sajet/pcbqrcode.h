#ifndef PCBQRCODE_H
#define PCBQRCODE_H

#include <QWidget>

namespace Ui {
class PcbQrcode;
}

class PcbQrcode : public QWidget
{
    Q_OBJECT

public:
    explicit PcbQrcode(QWidget *parent = nullptr);
    ~PcbQrcode();
    // 全局条件字典管理
    static void addCondition(const QString &key, const QString &value);
    static bool removeCondition(const QString &key, const QString &value);
    static void clearConditions();
    static QStringList getConditionValues(const QString &key);
    static QMap<QString, QStringList> getAllConditions();
    static bool conditionExists(const QString &key, const QString &value);
    void UpadteTableRow();

private slots:
    void on_lineEdit_returnPressed();
    void on_pushButtonSelect_clicked();
    void on_pushButtonClear_clicked();

    void on_pushButtonDelete_clicked();

    void on_pushButtonAdd_clicked();

private:
    Ui::PcbQrcode *ui;
    static QMap<QString, QStringList> m_conditions;   // 全局条件字典
};

#endif // PCBQRCODE_H
