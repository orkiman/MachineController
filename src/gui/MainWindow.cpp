#include "gui/MainWindow.h"
#include "ui_MainWindow.h"
#include "gui/SettingsWindow.h"
#include "Logger.h"
#include "json.hpp"
#include "communication/ArduinoProtocol.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTableWidget>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QWidget>

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

    // Rebuild test table when guns/controller enable states change in SettingsWindow
    connect(settingsWindow_, &SettingsWindow::glueGunsChanged, this, [this]() {
        buildGlueTestTable();
    });

    // no need to manuly connect signals to slots it's done automatically
    // Connect signals to slots using the ui pointer
    // connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::on_runButton_clicked);
    // connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::on_stopButton_clicked);
    // connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::on_settingsButton_clicked);
    // connect(ui->clearMessageAreaButton, &QPushButton::clicked, this, &MainWindow::on_clearMessageAreaButton_clicked);
    
    // Use Qt::QueuedConnection to ensure this runs after the event loop starts
    getLogger()->debug("[MainWindow] Queuing emitWindowReady()...");

    // Build the glue test table on the right
    buildGlueTestTable();

    QMetaObject::invokeMethod(this, "emitWindowReady", Qt::QueuedConnection);
    getLogger()->debug("[MainWindow] emitWindowReady() queued.");

    getLogger()->debug("[MainWindow] Constructor finished");
}

// Slot: update the barcode table whenever Logic publishes a new snapshot
void MainWindow::onBarcodeStoreUpdated(const QMap<QString, QStringList>& store) {
    try {
        renderBarcodeTable(store);
    } catch (const std::exception& e) {
        getLogger()->error("[MainWindow::onBarcodeStoreUpdated] Exception: {}", e.what());
    }
}

// Helper to render index + selected channels based on settings
void MainWindow::renderBarcodeTable(const QMap<QString, QStringList>& store) {
    QTableWidget* table = findChild<QTableWidget*>("barcodeTable");
    if (!table || !config_) return;

    // Read settings
    int rows = config_->getNumberOfMachineCells();
    if (rows < 0) rows = 0;
    int maxChannels = config_->getBarcodeChannelsToShow();
    if (maxChannels < 0) maxChannels = 0;

    // Build an ordered list of channel names to show (prioritize active comms from config)
    QStringList selected;
    nlohmann::json comm = config_->getCommunicationSettings();
    // 1) Active comms in config
    for (auto it = comm.begin(); it != comm.end(); ++it) {
        const std::string name = it.key();
        const bool active = it.value().value("active", true);
        if (!active) continue;
        QString qname = QString::fromStdString(name);
        selected.push_back(qname);
        if (selected.size() >= maxChannels) break;
    }
    // 2) If not enough, add any remaining store keys
    if (selected.size() < maxChannels) {
        for (auto it = store.begin(); it != store.end() && selected.size() < maxChannels; ++it) {
            if (!selected.contains(it.key())) selected.push_back(it.key());
        }
    }

    // Prepare columns: 1 index + N channels
    const int columns = 1 + selected.size();
    table->clear();
    table->setRowCount(rows);
    table->setColumnCount(columns);

    // Headers: Index + channel labels (use description if available)
    QStringList headers;
    headers << "Index";
    for (const auto& ch : selected) {
        QString label = ch; // default to communication name
        const std::string chs = ch.toStdString();
        if (comm.contains(chs) && comm[chs].contains("description")) {
            label = QString::fromStdString(comm[chs]["description"].get<std::string>());
        }
        headers << label;
    }
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    // Fill rows
    for (int r = 0; r < rows; ++r) {
        // Index column
        auto* idxItem = new QTableWidgetItem(QString::number(r));
        idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(r, 0, idxItem);

        // Channel columns
        for (int c = 0; c < selected.size(); ++c) {
            const QString& ch = selected[c];
            QString text;
            if (store.contains(ch)) {
                const QStringList& list = store[ch];
                if (r >= 0 && r < list.size()) text = list[r];
            }
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(r, 1 + c, item);
        }
    }

    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

// Build and populate the right-side glue test table
void MainWindow::buildGlueTestTable() {
    QTableWidget* table = findChild<QTableWidget*>("glueTestTable");
    if (!table) {
        getLogger()->warn("[MainWindow] glueTestTable widget not found in UI");
        return;
    }

    table->clear();
    table->setColumnCount(5);
    QStringList headers; headers << "Controller" << "G1" << "G2" << "G3" << "G4";
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(false);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    if (!config_) return;

    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || !glueSettings["controllers"].is_object()) {
            table->setRowCount(0);
            return;
        }

        const auto& controllers = glueSettings["controllers"];

        // First, count enabled controllers that have a communication port
        int rowCount = 0;
        for (const auto& [controllerName, controller] : controllers.items()) {
            bool enabled = controller.value("enabled", true);
            std::string comm = controller.value("communication", "");
            if (enabled && !comm.empty()) rowCount++;
        }

        table->setRowCount(rowCount);

        int row = 0;
        for (const auto& [controllerName, controller] : controllers.items()) {
            bool controllerEnabled = controller.value("enabled", true);
            if (!controllerEnabled) continue;

            std::string comm = controller.value("communication", "");
            if (comm.empty()) continue; // skip controllers without a port

            std::string activePlan = controller.value("activePlan", "");

            // Determine gun enabled flags from active plan
            bool gunActive[4] = {false,false,false,false};
            if (!activePlan.empty() && controller.contains("plans") && controller["plans"].contains(activePlan)) {
                const auto& plan = controller["plans"][activePlan];
                if (plan.contains("guns") && plan["guns"].is_array()) {
                    size_t idx = 0;
                    for (const auto& gun : plan["guns"]) {
                        if (idx < 4) gunActive[idx] = gun.value("enabled", true);
                        idx++;
                    }
                }
            }

            // Column 0: Controller name
            auto* nameItem = new QTableWidgetItem(QString::fromStdString(controllerName));
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, 0, nameItem);

            // Columns 1..4: gun toggles (hidden if gun disabled)
            for (int g = 1; g <= 4; ++g) {
                bool enabledGun = gunActive[g-1];
                if (!enabledGun) {
                    // Put an empty, marginless widget so the cell stays compact; no checkbox shown
                    QWidget* emptyCell = new QWidget(table);
                    QHBoxLayout* layout = new QHBoxLayout(emptyCell);
                    layout->setContentsMargins(0,0,0,0);
                    layout->addStretch();
                    emptyCell->setLayout(layout);
                    table->setCellWidget(row, g, emptyCell);
                    continue;
                }

                QCheckBox* cb = new QCheckBox(table);
                cb->setChecked(false);
                // Center the checkbox
                QWidget* cell = new QWidget(table);
                QHBoxLayout* layout = new QHBoxLayout(cell);
                layout->addWidget(cb);
                layout->setAlignment(cb, Qt::AlignCenter);
                layout->setContentsMargins(0,0,0,0);
                cell->setLayout(layout);
                table->setCellWidget(row, g, cell);

                // Capture needed values for sending messages
                std::string commPort = comm;
                int gunIndex = g;
                QString ctrlNameQ = QString::fromStdString(controllerName);
                connect(cb, &QCheckBox::toggled, this, [this, commPort, gunIndex, ctrlNameQ](bool on){
                    std::string msg = ArduinoProtocol::createTestMessage(gunIndex, on);
                    if (!msg.empty()) {
                        ArduinoProtocol::sendMessage(eventQueue_, commPort, msg);
                        addMessage(QString("Sent test %1 for %2 G%3 -> %4")
                            .arg(on ? "ON" : "OFF")
                            .arg(ctrlNameQ)
                            .arg(gunIndex)
                            .arg(QString::fromStdString(msg)));
                    }
                });
            }

            row++;
        }

        table->resizeColumnsToContents();
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int c=1;c<5;++c) table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    } catch (const std::exception& e) {
        getLogger()->error("[MainWindow::buildGlueTestTable] Exception: {}", e.what());
    }
}

void MainWindow::on_selectDataFileButton_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Data File", "", "Text Files (*.txt);;All Files (*)");
    if (!filePath.isEmpty()) {
        // Load file using config startPosition and endPosition
        if (dataFile_.loadFromFile(filePath.toStdString(), *config_)) {
            dataFilePath_ = filePath;
            ui->dataFilePathLabel->setText(filePath);
            // create gui event for loading the file
            GuiEvent event;
            event.keyword = "ParameterChange";
            event.target = "datafile";
            event.data = filePath.toStdString();
            eventQueue_.push(event);
        } else {
            dataFilePath_ = "";
            ui->dataFilePathLabel->setText("Failed to load file");
            QMessageBox::warning(this, "File Load Error", "Failed to load the selected data file.");
        }
    }
}

MainWindow::~MainWindow() {
    delete ui;
    delete settingsWindow_;
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
