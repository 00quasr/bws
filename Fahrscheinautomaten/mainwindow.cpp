#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Fahrscheinautomat");
    connect(ui->leEinwurf, &QLineEdit::returnPressed,
            this, &MainWindow::on_leEinwurf_eingabe);
}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_leEinwurf_eingabe()
{
    double euro = ui->lePreis->text().toDouble();
    double einwurfEuro = ui->leEinwurf->text().toDouble();

    int preis = euro * 100;
    int einwurf = einwurfEuro * 100;

    ui->lwAnzeige->clear();

    if (einwurf < euro){
        ui->lwAnzeige->addItem("Einwurf zu wenig!");
        int differenz = preis - einwurf;
        QString txt = "Es fehlen noch" + QString::number(differenz / 100.0) + "Euro";
        ui->lwAnzeige->addItem(txt);
        return;
    }

    if (einwurf == euro){
        ui->lwAnzeige->addItem("kein rückgeld");
        return;
    }

    int rueckgeld = einwurf - preis;

    QString txt = "Rückgeld: " + QString::number(rueckgeld / 100.0, 'f', 2) + " Euro";


    int muenzen[] = {200, 100, 50, 20, 10, 5, 2, 1};
    QString Name[] = {"2 Euro", "1 Euro", "50 Cent", "20 Cent", "10 Cent", "5 Cent", "2 Cent", "1 Cent"};

    for (int i = 0; i < 8; i++){
        int anzahl = rueckgeld / muenzen[i];

        if (anzahl > 0){
            txt = QString::number(anzahl) + " x " + Name[i];
            ui->lwAnzeige->addItem(txt);
            rueckgeld = rueckgeld % muenzen[i];
        }
    }
}
