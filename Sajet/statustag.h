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

    void update_table_travel(QString serial_number);
    void update_table_parts(QString serial_number);
};

#endif // STATUSTAG_H
