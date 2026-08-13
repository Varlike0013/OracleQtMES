#ifndef TGSGROUP_H
#define TGSGROUP_H

#include <QWidget>
#include <QTreeWidgetItem>

namespace Ui {
class TGSGroup;
}
struct ProcParams {
    QStringList inParams;   // 输入参数名（大写）
    QStringList outParams;  // 输出参数名（大写）
};
struct GroupJobInfo {
    QString groupId;
    QString groupName;
    QString jobId;
    QString groupSeq;
    QString seqElse;
    QString seqOther;
    QString valueKind;
    QString typeId;
    QString typeNameE;
    QString procCallName;
};
struct JobDetailInfo {
    QString jobSeq;
    QString sprocName;
};
class TGSGroup : public QWidget
{
    Q_OBJECT

public:
    explicit TGSGroup(QWidget *parent = nullptr);
    ~TGSGroup();

private slots:
    void on_lineEditInput_returnPressed();
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void on_pushButtonBuild_clicked();
    void on_pushButtonExecute_clicked();
    void on_comboBoxLine_currentTextChanged(const QString &arg1);
    void on_comboBoxProcess_currentTextChanged(const QString &arg1);
    void on_lineEditGroupID_returnPressed();

    void on_lineEditTrev_returnPressed();

private:
    Ui::TGSGroup *ui;
    QString m_currentProc;
    QMap<QString, QString> m_dataMap;  // 全局键值对存储
    QMap<QString, QLineEdit*> m_inputEdits; // 输入参数名 -> 输入控件
    QList<JobDetailInfo> m_jobDetailList;
    QList<GroupJobInfo> m_groupJobList;
    int m_currentStep;      // 当前步骤（从 0 开始）
    int m_maxStep;          // 最大步骤（等于查询结果数量）
    QString getProcedureSource(const QString &procName);
    ProcParams getProcedureParams(const QString &procName);
    void loadPDline();
    void fetchJobDetails(QString groupid, QString jobid);
};

#endif // TGSGROUP_H
