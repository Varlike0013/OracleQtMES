#ifndef REWORKQUESTION_H
#define REWORKQUESTION_H

#include <QWidget>
#include <QSqlQueryModel>

namespace Ui {
class ReworkQuestion;
}

class ReworkQuestion : public QWidget
{
    Q_OBJECT

public:
    explicit ReworkQuestion(QWidget *parent = nullptr);
    ~ReworkQuestion();

private slots:
    void on_selectButton_clicked();

    void on_replyButton_clicked();

    void on_tableView_doubleClicked(const QModelIndex &index);

private:
    Ui::ReworkQuestion *ui;
    QSqlQueryModel *m_model = nullptr;
    void update_table();
};

#endif // REWORKQUESTION_H
