/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.9.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QSpinBox *boardNum;
    QPushButton *setVolt;
    QPushButton *monChan;
    QLineEdit *measVolt;
    QLineEdit *measAmp;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;
    QLabel *label_4;
    QLabel *label_5;
    QLabel *label_6;
    QLabel *label_7;
    QSpinBox *chanNum;
    QLineEdit *desiredVolt;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QStringLiteral("MainWindow"));
        MainWindow->resize(360, 319);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        boardNum = new QSpinBox(centralwidget);
        boardNum->setObjectName(QStringLiteral("boardNum"));
        boardNum->setGeometry(QRect(20, 50, 61, 31));
        boardNum->setMinimum(1);
        boardNum->setMaximum(4);
        setVolt = new QPushButton(centralwidget);
        setVolt->setObjectName(QStringLiteral("setVolt"));
        setVolt->setGeometry(QRect(30, 110, 89, 25));
        monChan = new QPushButton(centralwidget);
        monChan->setObjectName(QStringLiteral("monChan"));
        monChan->setGeometry(QRect(170, 110, 131, 25));
        measVolt = new QLineEdit(centralwidget);
        measVolt->setObjectName(QStringLiteral("measVolt"));
        measVolt->setGeometry(QRect(220, 200, 113, 25));
        measAmp = new QLineEdit(centralwidget);
        measAmp->setObjectName(QStringLiteral("measAmp"));
        measAmp->setGeometry(QRect(220, 240, 113, 25));
        label = new QLabel(centralwidget);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 20, 61, 16));
        label_2 = new QLabel(centralwidget);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(110, 20, 71, 17));
        label_3 = new QLabel(centralwidget);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(210, 20, 121, 17));
        label_4 = new QLabel(centralwidget);
        label_4->setObjectName(QStringLiteral("label_4"));
        label_4->setGeometry(QRect(330, 50, 67, 31));
        label_4->setMinimumSize(QSize(67, 17));
        label_4->setStyleSheet(QStringLiteral(""));
        label_5 = new QLabel(centralwidget);
        label_5->setObjectName(QStringLiteral("label_5"));
        label_5->setGeometry(QRect(20, 160, 211, 17));
        label_6 = new QLabel(centralwidget);
        label_6->setObjectName(QStringLiteral("label_6"));
        label_6->setGeometry(QRect(20, 200, 181, 17));
        label_7 = new QLabel(centralwidget);
        label_7->setObjectName(QStringLiteral("label_7"));
        label_7->setGeometry(QRect(20, 240, 141, 17));
        chanNum = new QSpinBox(centralwidget);
        chanNum->setObjectName(QStringLiteral("chanNum"));
        chanNum->setGeometry(QRect(110, 50, 61, 31));
        chanNum->setMaximum(7);
        desiredVolt = new QLineEdit(centralwidget);
        desiredVolt->setObjectName(QStringLiteral("desiredVolt"));
        desiredVolt->setGeometry(QRect(210, 50, 113, 25));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QStringLiteral("menubar"));
        menubar->setGeometry(QRect(0, 0, 360, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QStringLiteral("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "SiPM Power Supply Control", Q_NULLPTR));
        setVolt->setText(QApplication::translate("MainWindow", "Set Voltage", Q_NULLPTR));
        monChan->setText(QApplication::translate("MainWindow", "Monitor Channel", Q_NULLPTR));
        measVolt->setText(QString());
        label->setText(QApplication::translate("MainWindow", "Board #", Q_NULLPTR));
        label_2->setText(QApplication::translate("MainWindow", "Channel #", Q_NULLPTR));
        label_3->setText(QApplication::translate("MainWindow", "Desired Voltage", Q_NULLPTR));
        label_4->setText(QApplication::translate("MainWindow", "<html><head/><body><p><span style=\" font-size:14pt;\">V</span></p></body></html>", Q_NULLPTR));
        label_5->setText(QApplication::translate("MainWindow", "Monitored / Measured Values", Q_NULLPTR));
        label_6->setText(QApplication::translate("MainWindow", "SiPM Voltage (volts)", Q_NULLPTR));
        label_7->setText(QApplication::translate("MainWindow", "SiPM Current (amps)", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
