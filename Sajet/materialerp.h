#ifndef MATERIALERP_H
#define MATERIALERP_H

#include <QWidget>
#include <QItemSelection>

namespace Ui {
class MaterialErp;
}

class MaterialErp : public QWidget
{
    Q_OBJECT

public:
    explicit MaterialErp(QWidget *parent = nullptr);
    ~MaterialErp();

private slots:
    void on_lineEditSelect_returnPressed();
    void onCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void on_pushButtonUpdate_clicked();
    void on_pushButtonAdd_clicked();
    void on_pushButtonDelete_clicked();

private:
    Ui::MaterialErp *ui;
};

#endif // MATERIALERP_H
