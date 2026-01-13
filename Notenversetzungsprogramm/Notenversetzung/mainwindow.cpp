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

void MainWindow::on_btnBerechnen_clicked()
{
    double mathe = ui->spinMathe->value();
    double englisch = ui->spinEnglisch->value();
    double deutsch = ui->spinDeutsch->value();
    double sport = ui->spinSport->value();
    double religion = ui->spinReligion->value();

    int schlechteHauptfaecher = 0;

    double durchschnitt = (mathe + englisch + deutsch + sport + religion) / 5.0;


    if (mathe > 4)
        schlechteHauptfaecher = schlechteHauptfaecher + 1;
    if (englisch > 4)
        schlechteHauptfaecher = schlechteHauptfaecher + 1;
    if (deutsch > 4)
        schlechteHauptfaecher = schlechteHauptfaecher + 1;


    if (durchschnitt < 4.5) {
        if (schlechteHauptfaecher < 2) {
            ui->lblErgebnis->setText("Versetzt!");
            ui->lblErgebnis->setStyleSheet("QLabel { background-color: lightgreen; padding: 10px; }");
        }
        else {
            ui->lblErgebnis->setText("Schuljahr muss wiederholt werden");
            ui->lblErgebnis->setStyleSheet("QLabel { background-color: lightcoral; padding: 10px; }");
        }
    }
    else {
        ui->lblErgebnis->setText("Schuljahr muss wiederholt werden");
        ui->lblErgebnis->setStyleSheet("QLabel { background-color: lightcoral; padding: 10px; }");
    }
}
