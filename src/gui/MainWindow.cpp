#include "gui/MainWindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // Signals and slots connections (optional, usually automatic)
    connect(ui.buttonStart, &QPushButton::clicked, this, &MainWindow::on_buttonStart_clicked);
}

MainWindow::~MainWindow() { }

void MainWindow::on_buttonStart_clicked()
{
    QMessageBox::information(this, "Info", "Start button clicked!");
}
