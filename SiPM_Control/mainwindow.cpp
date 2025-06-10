#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <cmath>

extern "C" {
 #include "sipmControl.h"
}

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

void MainWindow::on_setVolt_clicked()
{
    int boardNum;
    int chanNum;
    double desVolt;

    boardNum = ui->boardNum->value();
    chanNum = ui->chanNum->value();
    QString desVoltStr = ui->desiredVolt->text();
    desVolt = desVoltStr.toDouble();
    qDebug("desvolt = %f", desVolt);

    if ((desVolt < -32) | (desVolt > 0))
    {
        QMessageBox::warning(this, "Error! Invalid voltage entry!", "The voltage must be between 0 and -32!");
        desVolt = 0;
        return;
    }
    else
    {
        desVolt = std::abs(desVolt);
    }

    gui_set(boardNum, chanNum, desVolt);

    return;
}

void MainWindow::on_monChan_clicked()
{
    double current = 0.0;
    double refVolt = 0.0;
    int boardNum;
    int chanNum;
    int nakFlag = 0; // if 0 then no nak on LTC2451, if 1 then there was a nak, this could be a bool but nah

    boardNum = ui->boardNum->value();
    chanNum = ui->chanNum->value();

    gui_monitor(boardNum, chanNum, &current, &refVolt, &nakFlag);

    QString currStr = QString("%1").arg(current, 0, 'G', 4);
    QString refStr = QString("%1").arg(refVolt, 0, 'G', 4);

    if (nakFlag == 1) {
        QMessageBox::warning(this, "Error!", " Got I2C NAK/NACK while reading from LTC2451 ADC!\n DO NO TRUST THE MEASURED VALUES!\n Diagnose the issue or retry measurement!");
    }
    else {}

    ui->measAmp->setText(currStr);
    ui->measVolt->setText(refStr);

    return;
}
