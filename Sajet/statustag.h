#ifndef STATUSTAG_H
#define STATUSTAG_H

#include <QWidget>

namespace Ui {
class StatusTag;
}

class StatusTag : public QWidget
{
    Q_OBJECT

public:
    explicit StatusTag(QWidget *parent = nullptr);
    ~StatusTag();

private slots:
    void on_lineEdit_input_returnPressed();

private:
    Ui::StatusTag *ui;
};

#endif // STATUSTAG_H
