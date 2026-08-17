#ifndef TGSGROUPTEST_H
#define TGSGROUPTEST_H

#include <QWidget>

namespace Ui {
class TGSGroupTest;
}
struct GroupJobInfo {
    QString groupId;
    QString groupName;
    QString jobId;
    QString jobDesc;
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
class TGSGroupTest : public QWidget
{
    Q_OBJECT

public:
    explicit TGSGroupTest(QWidget *parent = nullptr);
    ~TGSGroupTest();

private slots:
    void on_comboBoxLine_currentTextChanged(const QString &arg1);
    void on_comboBoxProcess_currentTextChanged(const QString &arg1);
    void on_lineEditGroupID_returnPressed();
    void on_lineEditTrev_returnPressed();
    void on_comboBoxName_currentIndexChanged(int index);

private:
    Ui::TGSGroupTest *ui;
    QList<JobDetailInfo> m_jobDetailList;
    QList<GroupJobInfo> m_groupJobList;
    QMap<QString, QString> m_dataMap;  // 全局键值对存储
    int m_currentStep;      // 当前步骤（从 0 开始）
    int m_maxStep;          // 最大步骤（等于查询结果数量）
    void fetchJobDetails(QString groupid, QString jobid);
    void executeJobProc(QString TREV,QString procCallname);
};

#endif // TGSGROUPTEST_H
