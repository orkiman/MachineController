#include "gui/MainWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // no need to manuly connect signals to slots it's done automatically
    // Connect signals to slots using the ui pointer
    // connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::on_runButton_clicked);
    // connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked);
    // connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::on_settingsButton_clicked);
    // connect(ui->clearMessageAreaButton, &QPushButton::clicked, this, &MainWindow::on_clearMessageAreaButton_clicked);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_runButton_clicked() {
    qDebug() << "Run button clicked";
}

void MainWindow::on_stopButton_clicked() {
    qDebug() << "Stop button clicked";
}

void MainWindow::on_settingsButton_clicked() {
    // Your logic for showing the settings window
}

void MainWindow::on_clearMessageAreaButton_clicked() {
    ui->messageArea->clear();
}
