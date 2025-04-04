#include "gui/MainWindow.h"
#include "ui_MainWindow.h"
#include "gui/SettingsWindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      eventQueue_(eventQueue)
{
    ui->setupUi(this);

    // Create the settings window once
    settingsWindow_ = new SettingsWindow(this, eventQueue_);

    // no need to manuly connect signals to slots it's done automatically
    // Connect signals to slots using the ui pointer
    // connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::on_runButton_clicked);
    // connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked);
    // connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::on_settingsButton_clicked);
    // connect(ui->clearMessageAreaButton, &QPushButton::clicked, this, &MainWindow::on_clearMessageAreaButton_clicked);
}

MainWindow::~MainWindow() {
    delete ui;
    delete settingsWindow_;
}

void MainWindow::on_runButton_clicked() {
    qDebug() << "Run button clicked";
}

void MainWindow::on_stopButton_clicked() {
    qDebug() << "Stop button clicked";
}

void MainWindow::on_settingsButton_clicked() {
    // Show the settings window when the settings button is clicked
    if (settingsWindow_) {
        settingsWindow_->show();
    }
}

void MainWindow::on_clearMessageAreaButton_clicked() {
    ui->messageArea->clear();
}

void MainWindow::on_testButton_clicked() {
    eventQueue_.push(GuiEvent{GuiEventType::SendMessage, "Test message", "communication1"});
}
