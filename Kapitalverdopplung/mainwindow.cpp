#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Zinsrechner - Kapitalverdopplung");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnBerechnen_clicked()
{
    double kapital = ui->leStart->text().toDouble();
    double zinssatz = ui->lineEdit->text().toDouble();
    double startKapital = kapital;
    double zielKapital = startKapital * 2;
    int jahr = 0;

    ui->lwAnzeige->clear();

    QString txt = "Startkapital: " + QString::number(kapital, 'f', 2) + " Euro";
    ui->lwAnzeige->addItem(txt);
    ui->lwAnzeige->addItem("");

    while (kapital < zielKapital) {
        jahr = jahr + 1;
        kapital = kapital + (kapital * zinssatz / 100);

        txt = "nach " + QString::number(jahr) + " Jahr(en): "
              + QString::number(kapital, 'f', 2) + " Euro";
        ui->lwAnzeige->addItem(txt);
    }

    ui->lwAnzeige->addItem("");
    txt = "Kapital hat sich nach " + QString::number(jahr) + " Jahren verdoppelt";
    ui->lwAnzeige->addItem(txt);
}

void MainWindow::on_btnLeeren_clicked()
{
    ui->leStart->clear();
    ui->lineEdit->clear();
    ui->lwAnzeige->clear();
    ui->leStart->setFocus();
}

void MainWindow::on_btnEnde_clicked()
{
    this->close();
}
