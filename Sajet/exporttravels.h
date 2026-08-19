#ifndef EXPORTTRAVELS_H
#define EXPORTTRAVELS_H

#include <QWidget>

namespace Ui {
class ExportTravels;
}

class ExportTravels : public QWidget
{
    Q_OBJECT

public:
    explicit ExportTravels(QWidget *parent = nullptr);
    ~ExportTravels();

private:
    Ui::ExportTravels *ui;
};

#endif // EXPORTTRAVELS_H
