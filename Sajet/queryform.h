#ifndef QUERYFORM_H
#define QUERYFORM_H

#include <QWidget>
#include <QCoreApplication>
#include <QTreeWidgetItem>

namespace Ui {
class QueryForm;
}

class QueryForm : public QWidget
{
    Q_OBJECT

public:
    explicit QueryForm(QWidget *parent = nullptr);
    ~QueryForm();

private slots:
    void on_pushButtonAdd_clicked();
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void on_pushButtoBuild_clicked();
    void on_pushButtonExecute_clicked();
    void on_pushButtonExport_clicked();
    void on_pushButtonUpdate_clicked();
    void on_pushButtonDelete_clicked();
    void on_lineEditSelect_returnPressed();

private:
    Ui::QueryForm *ui;
    QStringList m_paramNames;               // 参数名称列表
    QMap<QString, QLineEdit*> m_paramEdits; // 参数名 -> 输入框
    QTreeWidgetItem *m_selectedItem = nullptr;
    static QString configFilePath();
    void loadSQL();
};

#endif // QUERYFORM_H
