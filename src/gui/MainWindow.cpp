#include "gui/MainWindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // Signals and slots connections (optional, usually automatic)
    // connect(ui.pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);
}

MainWindow::~MainWindow() { }

// this is called automatically when the button is clicked
// because When you call ui.setupUi(this), Qt automatically calls QMetaObject::connectSlotsByName(this)
// which connects the signals and slots based on the object names.
void MainWindow::on_pushButton_clicked()
{
    QMessageBox::information(this, "Info", "Start button clicked!");
}
