#ifndef CHECKEMP_H
#define CHECKEMP_H

#include <QWidget>

namespace Ui {
class CheckEMP;
}

class CheckEMP : public QWidget
{
    Q_OBJECT

public:
    explicit CheckEMP(QWidget *parent = nullptr);
    ~CheckEMP();

private slots:
    void on_lineEditInput_returnPressed();
    void on_tableViewEmp_clicked(const QModelIndex &index);
    void on_pushButtonSave_clicked();
    void on_pushButtonClear_clicked();
    void on_pushButtonAll_clicked();
    void on_lineEditEmp_returnPressed();
    void on_pushButtonSubmit_clicked();

private:
    Ui::CheckEMP *ui;
    QString m_empName;
    QSet<int> m_roleIds;
    void loadRole();
};

#endif // CHECKEMP_H
