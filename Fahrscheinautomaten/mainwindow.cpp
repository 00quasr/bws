#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_leEinwurf_eingabe()
{
    double euro = ui->lePreis()->test.toDouble();
    double einwurfEuro = ui->leEinwurf()->test.toDouble();

    int preis = euro * 100;
    int einwurf = einwurfEuro * 100;

    ui->lwAnzeige->clear();

    if (einwurf > euro){

    }

    if (einwurf == euro){

    }


    for (int i = 0; i < 8; i++){

    }
}
