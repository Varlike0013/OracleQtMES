#ifndef REWORKQUESTION_H
#define REWORKQUESTION_H

#include <QWidget>

namespace Ui {
class ReworkQuestion;
}

class ReworkQuestion : public QWidget
{
    Q_OBJECT

public:
    explicit ReworkQuestion(QWidget *parent = nullptr);
    ~ReworkQuestion();

private:
    Ui::ReworkQuestion *ui;
};

#endif // REWORKQUESTION_H
