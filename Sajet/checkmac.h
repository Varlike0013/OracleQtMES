#ifndef CHECKMAC_H
#define CHECKMAC_H

#include <QWidget>

namespace Ui {
class CheckMac;
}

class CheckMac : public QWidget
{
    Q_OBJECT

public:
    explicit CheckMac(QWidget *parent = nullptr);
    ~CheckMac();

private:
    Ui::CheckMac *ui;
};

#endif // CHECKMAC_H
