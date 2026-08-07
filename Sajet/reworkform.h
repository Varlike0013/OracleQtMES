#ifndef REWORKFORM_H
#define REWORKFORM_H

#include <QWidget>

namespace Ui {
class ReworkForm;
}

class ReworkForm : public QWidget
{
    Q_OBJECT

public:
    explicit ReworkForm(QWidget *parent = nullptr);
    ~ReworkForm();
    // 全局条件字典管理
    static void addCondition(const QString &key, const QString &value);
    static bool removeCondition(const QString &key, const QString &value);
    static void clearConditions();
    static QStringList getConditionValues(const QString &key);
    static QMap<QString, QStringList> getAllConditions();
    static bool conditionExists(const QString &key, const QString &value);
    void UpadteTable();

private slots:
    void on_pushButtonNew_clicked();
    void on_lineEditInput_returnPressed();
    void on_lineEditRoute_returnPressed();
    void on_pushButtonReady_clicked();
    void on_lineEditWo_returnPressed();
    void on_checkBoxWo_stateChanged(int arg1);
    void on_pushButtonClear_clicked();

private:
    Ui::ReworkForm *ui;
    static QMap<QString, QStringList> m_conditions;   // 全局条件字典
    bool addInputQty(const QString &wo, int qty);
    bool checkWoQtyEnough(const QString &wo, int needQty, int *remaining = nullptr);
};

#endif // REWORKFORM_H
