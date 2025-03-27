#include "gui/SettingsWindow.h"
#include "ui_SettingsWindow.h"
#include <QDebug>

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SettingsWindow)
{
    ui->setupUi(this);

    // Connect signals and slots using the ui pointer
    connect(ui->overrideOutputsCheckBox, &QCheckBox::stateChanged,
            this, &SettingsWindow::on_overrideOutputsCheckBox_stateChanged);
    connect(ui->applyButton, &QPushButton::clicked,
            this, &SettingsWindow::on_applyButton_clicked);
    connect(ui->cancelButton, &QPushButton::clicked,
            this, &SettingsWindow::on_cancelButton_clicked);
    connect(ui->defaultsButton, &QPushButton::clicked,
            this, &SettingsWindow::on_defaultsButton_clicked);
}

SettingsWindow::~SettingsWindow() {
    delete ui;
}

void SettingsWindow::on_overrideOutputsCheckBox_stateChanged(int state) {
    // Set the dynamic property "overrideOutputs" based on the checkbox state
    setProperty("overrideOutputs", state == Qt::Checked);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void SettingsWindow::on_applyButton_clicked() {
    qDebug() << "Apply button clicked";
}

void SettingsWindow::on_cancelButton_clicked() {
    qDebug() << "Cancel button clicked";
    close();
}

void SettingsWindow::on_defaultsButton_clicked() {
    qDebug() << "Defaults button clicked";
}
