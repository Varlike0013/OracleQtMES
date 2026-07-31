#include "checkmac.h"
#include "ui_checkmac.h"

CheckMac::CheckMac(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CheckMac)
{
    ui->setupUi(this);
}

CheckMac::~CheckMac()
{
    delete ui;
}
