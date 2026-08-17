#ifndef TGSGROUP_H
#define TGSGROUP_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QTextCursor>

namespace Ui {
class TGSGroup;
}
struct ProcParams {
    QStringList inParams;   // 输入参数名（大写）
    QStringList outParams;  // 输出参数名（大写）
};
class TGSGroup : public QWidget
{
    Q_OBJECT

public:
    explicit TGSGroup(QWidget *parent = nullptr);
    ~TGSGroup();
    static ProcParams getProcedureParams(const QString &procName);

private slots:
    void on_lineEditInput_returnPressed();
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void on_pushButtonBuild_clicked();
    void on_pushButtonExecute_clicked();
    void on_lineEditQuery_returnPressed();
    void on_pushButtonPrevious_clicked();
    void on_pushButtonNext_clicked();

private:
    Ui::TGSGroup *ui;
    QString m_currentProc;
    QMap<QString, QLineEdit*> m_inputEdits; // 输入参数名 -> 输入控件
    QString getProcedureSource(const QString &procName);
    QList<QTextCursor> m_matchPositions;
    int m_currentMatchIndex = -1;
    void highlightAllMatches(const QString &text);
};

#endif // TGSGROUP_H
