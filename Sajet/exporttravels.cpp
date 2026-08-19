#include "exporttravels.h"
#include "ui_exporttravels.h"

ExportTravels::ExportTravels(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ExportTravels)
{
    ui->setupUi(this);
}

ExportTravels::~ExportTravels()
{
    delete ui;
}
