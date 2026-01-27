#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Heron-Verfahren - Quadratwurzel");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnBerechnen_clicked(){
    double zahl = ui->leZahl->text().toDouble();
    int genauigkeit = ui->leKomma->text().toDouble();

    int zähler =0;
    double xOld = zahl;
    double xNew = 0;
    double toleranz = 1.0;

    for (int i = 0; i <= genauigkeit; i++){
        toleranz = toleranz / 10.0;
    }

    ui->lwAnzeige->clear();

    if (zahl < 0){
        ui->lwAnzeige->addItem("Keine negativen Zalen eingeben");
        return;
    }

    if (zahl == 0) {
        ui->lwAnzeige->addItem("Wurzel aus 0 ist gleich = 0");
        return;
    }

    QString txt = "zahl :" + QString::number(zahl);
    ui->lwAnzeige->addItem(txt);

    do{
        xNew = 0.5 * (xOld + zahl / xOld);
        zähler++;

        txt = "Iteration " + QString::number(zähler) + ": x = " + QString::number(xOld, 'f', genauigkeit + 2);
        ui->lwAnzeige->addItem(txt);

        double differenz = xNew - xOld;
        if (differenz < 0) {
            differenz = -differenz;
        }

        if (differenz < toleranz) {
            break;
        }

        xOld = xNew;


    }while(true);

    txt = "√" + QString::number(zahl) + " = " + QString::number(xNew, 'f', genauigkeit);
    ui->lwAnzeige->addItem(txt);


}


void MainWindow::on_btnClear_clicked()
{
    ui->leZahl->clear();
    ui->leKomma->clear();
    ui->lwAnzeige->clear();
    ui->leZahl->setFocus();
}
