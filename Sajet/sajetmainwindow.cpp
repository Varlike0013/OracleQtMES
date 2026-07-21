#include "sajetmainwindow.h"
#include "ui_sajetmainwindow.h"

SajetMainWindow::SajetMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SajetMainWindow)
{
    ui->setupUi(this);
}

SajetMainWindow::~SajetMainWindow()
{
    delete ui;
}
