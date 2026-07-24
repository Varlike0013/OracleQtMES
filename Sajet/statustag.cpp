#include "statustag.h"
#include "ui_statustag.h"

StatusTag::StatusTag(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusTag)
{
    ui->setupUi(this);
}

StatusTag::~StatusTag()
{
    delete ui;
}

void StatusTag::on_lineEdit_input_returnPressed()
{
    return;
}

