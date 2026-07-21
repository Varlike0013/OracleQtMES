#include "gedtmainwindow.h"
#include "ui_gedtmainwindow.h"

GedtMainWindow::GedtMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GedtMainWindow)
{
    ui->setupUi(this);
}

GedtMainWindow::~GedtMainWindow()
{
    delete ui;
}
