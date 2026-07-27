#include "reworkquestion.h"
#include "ui_reworkquestion.h"

ReworkQuestion::ReworkQuestion(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ReworkQuestion)
{
    ui->setupUi(this);
}

ReworkQuestion::~ReworkQuestion()
{
    delete ui;
}
