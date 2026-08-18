#ifndef REWORKFORM_H
#define REWORKFORM_H

#include <QWidget>
#include "conditionmanager.h"

namespace Ui {
class ReworkForm;
}

class ReworkForm : public QWidget
{
    Q_OBJECT

public:
    explicit ReworkForm(QWidget *parent = nullptr);
    ~ReworkForm();
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
    ConditionManager m_conditions;
    bool addInputQty(const QString &wo, int qty);
    bool checkWoQtyEnough(const QString &wo, int needQty, int *remaining = nullptr);
};

#endif // REWORKFORM_H
