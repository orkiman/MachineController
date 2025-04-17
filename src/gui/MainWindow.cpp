#include "gui/MainWindow.h"
#include "ui_MainWindow.h"
#include "gui/SettingsWindow.h"
#include "Logger.h"
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      eventQueue_(eventQueue),
      config_(&config)
{
    getLogger()->debug("[MainWindow] Constructor started");

    getLogger()->debug("[MainWindow] Calling ui->setupUi()...");
    ui->setupUi(this);
    getLogger()->debug("[MainWindow] ui->setupUi() finished.");

    // Create the settings window once and pass the Config object
    getLogger()->debug("[MainWindow] Creating SettingsWindow...");
    settingsWindow_ = new SettingsWindow(this, eventQueue_, config);
    getLogger()->debug("[MainWindow] SettingsWindow created.");

    // no need to manuly connect signals to slots it's done automatically
    // Connect signals to slots using the ui pointer
    // connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::on_runButton_clicked);
    // connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked);
    // connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::on_settingsButton_clicked);
    // connect(ui->clearMessageAreaButton, &QPushButton::clicked, this, &MainWindow::on_clearMessageAreaButton_clicked);
    
    // Use Qt::QueuedConnection to ensure this runs after the event loop starts
    getLogger()->debug("[MainWindow] Queuing emitWindowReady()...");
    QMetaObject::invokeMethod(this, "emitWindowReady", Qt::QueuedConnection);
    getLogger()->debug("[MainWindow] emitWindowReady() queued.");

    getLogger()->debug("[MainWindow] Constructor finished");
}

MainWindow::~MainWindow() {
    delete ui;
    delete settingsWindow_;
}

void MainWindow::on_runButton_clicked() {
    getLogger()->debug("Run button clicked");
}

void MainWindow::on_stopButton_clicked() {
    getLogger()->debug("Stop button clicked");
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
    // Create a GuiEvent to toggle LED blinking
    GuiEvent event;
    event.keyword = "SetVariable";
    event.target = "blinkLed0";
    
    // Push the event to the event queue
    eventQueue_.push(event);
    
    // Add a message to the GUI
    addMessage("Test button clicked - toggling LED blinking");
}

void MainWindow::addMessage(const QString& message, const QString& identifier) {
    // Add timestamp and format the message
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("[yyyy-MM-dd hh:mm:ss] ");
    
    // Create HTML formatted message with color based on identifier
    QString htmlMessage;
    
    if (identifier.toLower() == "error") {
        // Red for errors
        htmlMessage = QString("<span style=\"color: #FF0000;\">%1%2</span>").arg(timestamp).arg(message);
    } else if (identifier.toLower() == "warning" || identifier.toLower() == "warn") {
        // Orange for warnings
        htmlMessage = QString("<span style=\"color: #FFA500;\">%1%2</span>").arg(timestamp).arg(message);
    } else {
        // Default black for regular messages
        htmlMessage = QString("%1%2").arg(timestamp).arg(message);
    }
    
    // Append the formatted message to the message area
    ui->messageArea->append(htmlMessage);
}

void MainWindow::emitWindowReady() {
    // This method is called after the window is fully initialized and shown
    // Emit the signal to notify other components that the window is ready
    getLogger()->debug("[MainWindow] emitWindowReady() called.");
    getLogger()->debug("[MainWindow] Emitting windowReady signal...");
    emit windowReady();
    getLogger()->debug("[MainWindow] windowReady signal emitted.");
    getLogger()->debug("[MainWindow] emitWindowReady() finished.");
}
