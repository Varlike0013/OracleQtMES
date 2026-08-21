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

private slots:
    void on_pushButtonSelect_clicked();

    void on_comboBoxLine_currentTextChanged(const QString &arg1);

    void on_comboBoxProcess_currentTextChanged(const QString &arg1);

    void on_pushButtonExport_clicked();

private:
    Ui::ExportTravels *ui;
};

#endif // EXPORTTRAVELS_H
