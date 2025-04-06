#include "gui/SettingsWindow.h"
#include "ui_SettingsWindow.h"
#include "Config.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTimer>

SettingsWindow::SettingsWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config)
    : QDialog(parent),
      ui(new Ui::SettingsWindow),
      eventQueue_(eventQueue),
      config_(&config)
{
    ui->setupUi(this);
    
    // Fill the communication tab fields with default values
    fillCommunicationTabFields();
    
    // Fill the timers tab fields with values from config
    fillTimersTabFields();
    
    // Fill the IO tab with inputs and outputs from config
    fillIOTabFields();
    
    // Connect change events for all editable fields
    connectChangeEvents();
}

SettingsWindow::~SettingsWindow() {
    delete ui;
}

bool SettingsWindow::loadSettingsFromJson(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open settings file:" << filePath;
        eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Failed to load settings from " + filePath.toStdString(), "error"});
        return false;
    }
    
    QByteArray saveData = file.readAll();
    QJsonDocument loadDoc = QJsonDocument::fromJson(saveData);
    
    if (loadDoc.isNull() || !loadDoc.isObject()) {
        qDebug() << "Invalid JSON format in settings file";
        eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Invalid JSON format in settings file", "error"});
        file.close();
        return false;
    }
    
    QJsonObject json = loadDoc.object();
    
    // Check if communication settings exist
    if (!json.contains("communication") || !json["communication"].isObject()) {
        qDebug() << "No communication settings found in JSON";
        file.close();
        return false;
    }
    
    file.close();
    
    // Load timer settings
    fillTimersTabFields();
    
    return true;
}

void SettingsWindow::fillCommunicationTabFields() {
    // First set up all the combo boxes with their possible values
    // This needs to happen regardless of whether we load from JSON or use defaults
    
    // Communication 1 Tab setup
    // Port names - populate with available serial ports
    ui->portName1ComboBox->clear();
    ui->portName1ComboBox->addItems({"COM1", "COM2", "COM3", "COM4"});
    
    // Baud rates
    ui->baudRate1ComboBox->clear();
    ui->baudRate1ComboBox->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    
    // Parity options
    ui->parity1ComboBox->clear();
    ui->parity1ComboBox->addItems({"None", "Even", "Odd", "Mark", "Space"});
    
    // Data bits
    ui->dataBits1ComboBox->clear();
    ui->dataBits1ComboBox->addItems({"5", "6", "7", "8"});
    
    // Stop bits
    ui->stopBits1ComboBox->clear();
    ui->stopBits1ComboBox->addItems({"1", "1.5", "2"});
    
    // Communication 2 Tab setup
    // Port names
    ui->portName2ComboBox->clear();
    ui->portName2ComboBox->addItems({"COM1", "COM2", "COM3", "COM4"});
    
    // Baud rates
    ui->baudRate2ComboBox->clear();
    ui->baudRate2ComboBox->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    
    // Parity options
    ui->parity2ComboBox->clear();
    ui->parity2ComboBox->addItems({"None", "Even", "Odd", "Mark", "Space"});
    
    // Data bits
    ui->dataBits2ComboBox->clear();
    ui->dataBits2ComboBox->addItems({"5", "6", "7", "8"});
    
    // Stop bits
    ui->stopBits2ComboBox->clear();
    ui->stopBits2ComboBox->addItems({"1", "1.5", "2"});
    
    // Get communication settings from Config object
    if (!config_) {
        qDebug() << "Config object is null. Using default values.";
        fillWithDefaults();
        eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Config object is null. Using default values.", "warning"});
        return;
    }
    
    // Get communication settings using the same approach as RS232Communication
    auto commSettings = config_->getCommunicationSettings();
    
    // Check for communication1 key
    if (commSettings.contains("communication1")) {
        auto comm1 = commSettings["communication1"];
        
        // Port
        if (comm1.contains("port")) {
            QString port = QString::fromStdString(comm1["port"]);
            ui->portName1ComboBox->setCurrentText(port);
        } else if (comm1.contains("portName")) { // For backward compatibility
            QString portName = QString::fromStdString(comm1["portName"]);
            ui->portName1ComboBox->setCurrentText(portName);
        }
        
        // Baud rate
        if (comm1.contains("baudRate")) {
            int baudRate = comm1["baudRate"];
            ui->baudRate1ComboBox->setCurrentText(QString::number(baudRate));
        }
        
        // Parity
        if (comm1.contains("parity")) {
            QString parity = QString::fromStdString(comm1["parity"]);
            // Convert from single letter to full word
            if (parity == "N") ui->parity1ComboBox->setCurrentText("None");
            else if (parity == "E") ui->parity1ComboBox->setCurrentText("Even");
            else if (parity == "O") ui->parity1ComboBox->setCurrentText("Odd");
            else if (parity == "M") ui->parity1ComboBox->setCurrentText("Mark");
            else if (parity == "S") ui->parity1ComboBox->setCurrentText("Space");
            else ui->parity1ComboBox->setCurrentText(parity); // Direct setting if already in full word format
        }
        
        // Data bits
        if (comm1.contains("dataBits")) {
            int dataBits = comm1["dataBits"];
            ui->dataBits1ComboBox->setCurrentText(QString::number(dataBits));
        }
        
        // Stop bits
        if (comm1.contains("stopBits")) {
            double stopBits = comm1["stopBits"];
            ui->stopBits1ComboBox->setCurrentText(QString::number(stopBits));
        }
        
        // STX
        if (comm1.contains("stx")) {
            int stxValue = parseCharSetting(comm1, "stx", 2);
            QString stxText = QString::number(stxValue, 16).rightJustified(2, '0');
            ui->stx1LineEdit->setText(stxText);
        }
        
        // ETX
        if (comm1.contains("etx")) {
            int etxValue = parseCharSetting(comm1, "etx", 3);
            QString etxText = QString::number(etxValue, 16).rightJustified(2, '0');
            ui->etx1LineEdit->setText(etxText);
        }
        
        // Set test message
        if (comm1.contains("trigger")) {
            QString trigger = QString::fromStdString(comm1["trigger"]);
            ui->commuication1TriggerLineEdit->setText(trigger);
        }
    }
    
    // Apply settings from JSON for Communication 2
    if (commSettings.contains("communication2")) {
        auto comm2 = commSettings["communication2"];
        
        // Port
        if (comm2.contains("port")) {
            QString port = QString::fromStdString(comm2["port"]);
            ui->portName2ComboBox->setCurrentText(port);
        } else if (comm2.contains("portName")) { // For backward compatibility
            QString portName = QString::fromStdString(comm2["portName"]);
            ui->portName2ComboBox->setCurrentText(portName);
        }
        
        // Set baud rate
        if (comm2.contains("baudRate")) {
            int baudRate = comm2["baudRate"];
            ui->baudRate2ComboBox->setCurrentText(QString::number(baudRate));
        }
        
        // Parity
        if (comm2.contains("parity")) {
            QString parity = QString::fromStdString(comm2["parity"]);
            // Convert from single letter to full word
            if (parity == "N") ui->parity2ComboBox->setCurrentText("None");
            else if (parity == "E") ui->parity2ComboBox->setCurrentText("Even");
            else if (parity == "O") ui->parity2ComboBox->setCurrentText("Odd");
            else if (parity == "M") ui->parity2ComboBox->setCurrentText("Mark");
            else if (parity == "S") ui->parity2ComboBox->setCurrentText("Space");
            else ui->parity2ComboBox->setCurrentText(parity); // Direct setting if already in full word format
        }
        
        // Set data bits
        if (comm2.contains("dataBits")) {
            int dataBits = comm2["dataBits"];
            ui->dataBits2ComboBox->setCurrentText(QString::number(dataBits));
        }
        
        // Set stop bits
        if (comm2.contains("stopBits")) {
            double stopBits = comm2["stopBits"];
            ui->stopBits2ComboBox->setCurrentText(QString::number(stopBits));
        }
        
        // Set STX/ETX
        if (comm2.contains("stx")) {
            int stxValue = parseCharSetting(comm2, "stx", 2);
            QString stxText = QString::number(stxValue, 16).rightJustified(2, '0');
            ui->stx2LineEdit->setText(stxText);
        }
        
        if (comm2.contains("etx")) {
            int etxValue = parseCharSetting(comm2, "etx", 3);
            QString etxText = QString::number(etxValue, 16).rightJustified(2, '0');
            ui->etx2LineEdit->setText(etxText);
        }
        
        // Set test message
        if (comm2.contains("trigger")) {
            QString trigger = QString::fromStdString(comm2["trigger"]);
            ui->commuication2TriggerLineEdit->setText(trigger);
        }
    }
    

    
    // Set defaults for STX/ETX if not already set
    if (ui->stx2LineEdit->text().isEmpty()) {
        ui->stx2LineEdit->setText("02"); // Default if not specified
    }
    
    if (ui->etx2LineEdit->text().isEmpty()) {
        ui->etx2LineEdit->setText("03"); // Default if not specified
    }
    
    // Set trigger defaults only if they're not already set
    if (ui->commuication1TriggerLineEdit->text().isEmpty()) {
        ui->commuication1TriggerLineEdit->setText("t");
    }
    
    if (ui->commuication2TriggerLineEdit->text().isEmpty()) {
        ui->commuication2TriggerLineEdit->setText("t");
    }
    
    // Emit a message to the event queue that settings were loaded
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Communication settings loaded from JSON"});
}

void SettingsWindow::on_overrideOutputsCheckBox_stateChanged(int state) {
    // Set the dynamic property "overrideOutputs" based on the checkbox state
    bool enableOverrides = (state == Qt::Checked);
    setProperty("overrideOutputs", enableOverrides);
    style()->unpolish(this);
    style()->polish(this);
    update();
    
    // Enable or disable all output state checkboxes based on the override checkbox state
    for (int row = 0; row < ui->outputOverridesTable->rowCount(); ++row) {
        QWidget* widget = ui->outputOverridesTable->cellWidget(row, 2);
        if (widget) {
            QCheckBox* checkbox = widget->findChild<QCheckBox*>();
            if (checkbox) {
                checkbox->setEnabled(enableOverrides);
                if(!enableOverrides)
                checkbox->setChecked(false);
            }
        }
    }
    
    // Emit signal to notify Logic about the override state change
    emit outputOverrideStateChanged(enableOverrides);
    
    // If override is enabled, also send the current output states
    if (enableOverrides) {
        sendCurrentOutputStates();
    }
}

void SettingsWindow::on_applyButton_clicked() {
    qDebug() << "Apply button clicked";
    
    // Save settings to Config
    if (saveSettingsToConfig()) {
        eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Settings saved successfully", "info"});
        
        // Set the refreshing flag to prevent marking items as changed during refresh
        isRefreshing_ = true;
        
        // Reset all changed field markings after successful save
        resetChangedFields();
        
        // Refresh the timer fields to show the updated values
        fillTimersTabFields();
        
        // Reset the refreshing flag
        isRefreshing_ = false;
    } else {
        eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Failed to save settings", "error"});
    }
}
void SettingsWindow::on_cancelButton_clicked() {
    qDebug() << "Cancel button clicked";
    close();
}

void SettingsWindow::on_defaultsButton_clicked() {
    qDebug() << "Defaults button clicked";
    fillWithDefaults();
}



void SettingsWindow::on_refreshButton_clicked()
{
    qDebug() << "Refresh button clicked";
    fillIOTabFields();
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "IO states refreshed", "info"});
}

void SettingsWindow::updateInputStates(const std::unordered_map<std::string, IOChannel>& inputs)
{
    // Only update the states in the input table, don't recreate the entire table
    for (int row = 0; row < ui->inputStatesTable->rowCount(); ++row) {
        // Get the input name from the first column
        QTableWidgetItem* nameItem = ui->inputStatesTable->item(row, 0);
        if (nameItem) {
            std::string name = nameItem->text().toStdString();
            
            // Check if this input exists in the provided map
            auto it = inputs.find(name);
            if (it != inputs.end()) {
                // Get the current state in the table
                QTableWidgetItem* stateItem = ui->inputStatesTable->item(row, 2);
                if (stateItem) {
                    int currentState = stateItem->text().toInt();
                    int newState = it->second.state;
                    
                    // Only update and highlight if the state has actually changed
                    if (currentState != newState) {
                        stateItem->setText(QString::number(newState));
                        
                        // Highlight the row if the state has changed (visual feedback)
                        stateItem->setBackground(QBrush(QColor(255, 255, 0, 64))); // Light yellow highlight
                        
                        // Schedule the highlight to be removed after a short delay
                        QTimer* timer = new QTimer(this);
                        timer->setSingleShot(true);
                        connect(timer, &QTimer::timeout, [stateItem, timer]() {
                            stateItem->setBackground(QBrush());
                            timer->deleteLater(); // Clean up the timer
                        });
                        timer->start(500); // 500 ms delay
                    }
                }
            }
        }
    }
}


void SettingsWindow::fillIOTabFields() {
    if (!config_) {
        qDebug() << "Config object is null. Cannot load IO settings.";
        return;
    }
    
    try {
        // Clear the input states table
        ui->inputStatesTable->setRowCount(0);
        
        // Get inputs from config
        auto inputs = config_->getInputs();
        
        // Populate input states table
        for (const auto& input : inputs) {
            const std::string& name = input.first;
            const IOChannel& channel = input.second;
            
            // Add a new row to the table
            int row = ui->inputStatesTable->rowCount();
            ui->inputStatesTable->insertRow(row);
            
            // Create and set table items
            QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(name));
            QTableWidgetItem* descItem = new QTableWidgetItem(QString::fromStdString(channel.description));
            QTableWidgetItem* stateItem = new QTableWidgetItem(QString::number(channel.state));
            
            // Set the items as non-editable
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
            stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable);
            
            // Add items to the table
            ui->inputStatesTable->setItem(row, 0, nameItem);
            ui->inputStatesTable->setItem(row, 1, descItem);
            ui->inputStatesTable->setItem(row, 2, stateItem);
        }
        
        // Clear the output overrides table
        ui->outputOverridesTable->setRowCount(0);
        
        // Get outputs from config
        auto outputs = config_->getOutputs();
        
        // Populate output overrides table
        for (const auto& output : outputs) {
            const std::string& name = output.first;
            const IOChannel& channel = output.second;
            
            // Add a new row to the table
            int row = ui->outputOverridesTable->rowCount();
            ui->outputOverridesTable->insertRow(row);
            
            // Create and set table items
            QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(name));
            QTableWidgetItem* descItem = new QTableWidgetItem(QString::fromStdString(channel.description));
            
            // Create a checkbox for state
            QWidget* stateWidget = new QWidget();
            QCheckBox* stateCheckBox = new QCheckBox();
            QHBoxLayout* stateLayout = new QHBoxLayout(stateWidget);
            stateLayout->addWidget(stateCheckBox);
            stateLayout->setAlignment(Qt::AlignCenter);
            stateLayout->setContentsMargins(0, 0, 0, 0);
            stateWidget->setLayout(stateLayout);
            
            // Set initial state
            stateCheckBox->setChecked(channel.state != 0);
            stateCheckBox->setEnabled(false); // Initially disabled
            
            // Store the output name as property for later use when handling state changes
            stateCheckBox->setProperty("outputName", QString::fromStdString(name));
            
            // Set the items as non-editable
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
            
            // Add items to the table
            ui->outputOverridesTable->setItem(row, 0, nameItem);
            ui->outputOverridesTable->setItem(row, 1, descItem);
            ui->outputOverridesTable->setCellWidget(row, 2, stateWidget);
        }
        
        // Adjust column widths
        ui->inputStatesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui->outputOverridesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        
        // Disable state checkboxes if override is not enabled
        bool enableOverrides = ui->overrideOutputsCheckBox->isChecked();
        for (int row = 0; row < ui->outputOverridesTable->rowCount(); ++row) {
            QWidget* widget = ui->outputOverridesTable->cellWidget(row, 2);
            if (widget) {
                QCheckBox* checkbox = widget->findChild<QCheckBox*>();
                if (checkbox) {
                    checkbox->setEnabled(enableOverrides);
                    
                    // Disconnect any existing connections to avoid duplicates
                    disconnect(checkbox, &QCheckBox::stateChanged, nullptr, nullptr);
                    
                    // Connect checkbox state change to handle output overrides
                    // We use a lambda to capture the checkbox for each connection
                    // Note: Using stateChanged instead of checkStateChanged for compatibility
                    // with the existing UI connections, despite it being deprecated
                    connect(checkbox, &QCheckBox::stateChanged, [this, checkbox](int state) {
                        QString outputName = checkbox->property("outputName").toString();
                        if (!outputName.isEmpty()) {
                            // Call the handler method to process the output state change
                            handleOutputCheckboxStateChanged(outputName, state);
                        }
                    });
                }
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Exception while loading IO settings: " << e.what();
    }
}

// Collect and send current output states to Logic
void SettingsWindow::sendCurrentOutputStates() {
    // Create a map to hold the output states
    std::unordered_map<std::string, IOChannel> outputs;
    
    // Iterate through all rows in the output overrides table
    for (int row = 0; row < ui->outputOverridesTable->rowCount(); ++row) {
        // Get the output name from the first column
        QTableWidgetItem* nameItem = ui->outputOverridesTable->item(row, 0);
        if (nameItem) {
            std::string name = nameItem->text().toStdString();
            
            // Get the checkbox state from the third column
            QWidget* widget = ui->outputOverridesTable->cellWidget(row, 2);
            if (widget) {
                QCheckBox* checkbox = widget->findChild<QCheckBox*>();
                if (checkbox) {
                    // Create an IOChannel with the current state
                    IOChannel channel;
                    channel.state = checkbox->isChecked() ? 1 : 0;
                    
                    // Get description from the second column
                    QTableWidgetItem* descItem = ui->outputOverridesTable->item(row, 1);
                    if (descItem) {
                        channel.description = descItem->text().toStdString();
                    }
                    
                    // Add to the outputs map
                    outputs[name] = channel;
                }
            }
        }
    }
    
    // Emit the signal with the collected output states
    if (!outputs.empty()) {
        emit outputStateChanged(outputs);
    }
}

// Handle individual output checkbox state changes
void SettingsWindow::handleOutputCheckboxStateChanged(const QString& outputName, int state) {
    qDebug() << "Output override for " << outputName << ": " << (state == Qt::Checked ? "ON" : "OFF");
    
    // Send updated output states to Logic
    sendCurrentOutputStates();
}

void SettingsWindow::fillTimersTabFields()
{
    // Set the refreshing flag to prevent marking items as changed during loading
    isRefreshing_ = true;
    
    if (!config_) {
        qDebug() << "Config object is null. Cannot load timer settings.";
        isRefreshing_ = false; // Reset flag
        return;
    }
    
    try {
        // Clear the timers table
        ui->timersTable->setRowCount(0);
        
        // Get timer settings from config
        nlohmann::json timerSettings = config_->getTimerSettings();
        
        // Iterate through all timer entries
        for (auto it = timerSettings.begin(); it != timerSettings.end(); ++it) {
            const std::string& timerName = it.key();
            const nlohmann::json& timerData = it.value();
            
            // Skip if not an object
            if (!timerData.is_object()) {
                continue;
            }
            
            // Get timer properties
            int duration = timerData.value("duration", 1000);
            std::string description = timerData.value("description", "");
            
            // Add a new row to the table
            int row = ui->timersTable->rowCount();
            ui->timersTable->insertRow(row);
            
            // Create and set table items
            QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(timerName));
            QTableWidgetItem* durationItem = new QTableWidgetItem(QString::number(duration));
            QTableWidgetItem* descriptionItem = new QTableWidgetItem(QString::fromStdString(description));
            
            // Disable editing for name and description fields
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            descriptionItem->setFlags(descriptionItem->flags() & ~Qt::ItemIsEditable);
            
            // Add items to the table
            ui->timersTable->setItem(row, 0, nameItem);
            ui->timersTable->setItem(row, 1, durationItem);
            ui->timersTable->setItem(row, 2, descriptionItem);
            
            // Set background color for disabled fields to indicate they're not editable
            nameItem->setBackground(QBrush(QColor(240, 240, 240)));
            descriptionItem->setBackground(QBrush(QColor(240, 240, 240)));
        }
    } catch (const std::exception& e) {
        qDebug() << "Exception while loading timer settings: " << e.what();
    }
    
    // Reset the refreshing flag
    isRefreshing_ = false;
}

void SettingsWindow::fillWithDefaults()
{
    // Fill Communication 1 Tab with default values
    ui->portName1ComboBox->setCurrentText("COM1");
    ui->baudRate1ComboBox->setCurrentText("115200");
    ui->parity1ComboBox->setCurrentText("None");
    ui->dataBits1ComboBox->setCurrentText("8");
    ui->stopBits1ComboBox->setCurrentText("1");
    ui->stx1LineEdit->setText("02");
    ui->etx1LineEdit->setText("03");
    ui->commuication1TriggerLineEdit->setText("t");
    
    // Fill Communication 2 Tab with default values
    ui->portName2ComboBox->setCurrentText("COM2");
    ui->baudRate2ComboBox->setCurrentText("115200");
    ui->parity2ComboBox->setCurrentText("None");
    ui->dataBits2ComboBox->setCurrentText("8");
    ui->stopBits2ComboBox->setCurrentText("1");
    ui->stx2LineEdit->setText("02");
    ui->etx2LineEdit->setText("03");
    ui->commuication2TriggerLineEdit->setText("t");
    
    // Fill Timers Tab with default values
    ui->timersTable->setRowCount(0);
    
    // Add default timers
    // Timer 1
    int row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem1 = new QTableWidgetItem("timer1");
    QTableWidgetItem* durationItem1 = new QTableWidgetItem("1000");
    QTableWidgetItem* descriptionItem1 = new QTableWidgetItem("General purpose timer 1");
    ui->timersTable->setItem(row, 0, nameItem1);
    ui->timersTable->setItem(row, 1, durationItem1);
    ui->timersTable->setItem(row, 2, descriptionItem1);
    
    // Timer 2
    row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem2 = new QTableWidgetItem("timer2");
    QTableWidgetItem* durationItem2 = new QTableWidgetItem("2000");
    QTableWidgetItem* descriptionItem2 = new QTableWidgetItem("General purpose timer 2");
    ui->timersTable->setItem(row, 0, nameItem2);
    ui->timersTable->setItem(row, 1, durationItem2);
    ui->timersTable->setItem(row, 2, descriptionItem2);
    
    // Timer 3
    row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem3 = new QTableWidgetItem("timer3");
    QTableWidgetItem* durationItem3 = new QTableWidgetItem("5000");
    QTableWidgetItem* descriptionItem3 = new QTableWidgetItem("General purpose timer 3");
    ui->timersTable->setItem(row, 0, nameItem3);
    ui->timersTable->setItem(row, 1, durationItem3);
    ui->timersTable->setItem(row, 2, descriptionItem3);
    
    // Notify that default settings were loaded
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Default communication settings loaded", "settings"});
    
    // Prevent marking items as changed during refresh
    isRefreshing_ = true;
}

// Helper function to parse char settings (STX, ETX) similar to RS232Communication::parseCharSetting
int SettingsWindow::parseCharSetting(const nlohmann::json &settings, const std::string &key, int defaultValue) {
    if (!settings.contains(key)) {
        return defaultValue;
    }

    const auto &value = settings[key];
    if (value.is_number_integer()) {
        return value.get<int>();
    }
    else if (value.is_string()) {
        std::string strValue = value.get<std::string>();
        if (strValue.empty()) {
            return 0; // Empty string means no STX/ETX.
        }
        else if (strValue.rfind("0x", 0) == 0) {
            try {
                return std::stoi(strValue, nullptr, 16);
            } catch (const std::exception &) {
                qDebug() << "Invalid hex value for " << QString::fromStdString(key) << ": " << QString::fromStdString(strValue);
                return defaultValue;
            }
        }
        else {
            return strValue[0]; // Take the first character.
        }
    }
    else {
        qDebug() << "Invalid type for " << QString::fromStdString(key) << " setting. Using default value.";
        return defaultValue;
    }
}

bool SettingsWindow::saveSettingsToConfig() {
    if (!config_) {
        qDebug() << "Config object is null. Cannot save settings.";
        return false;
    }
    
    // We need to cast away the const-ness of config_ to modify it
    // This is safe because we know the Config object is owned by MainWindow and outlives SettingsWindow
    Config* mutableConfig = const_cast<Config*>(config_);
    
    // Create a new JSON object for communication settings
    nlohmann::json commSettings = nlohmann::json::object();
    
    // Update communication1 settings
    nlohmann::json comm1 = nlohmann::json::object();
    comm1["port"] = ui->portName1ComboBox->currentText().toStdString();
    comm1["description"] = "reader1"; // Default description
    comm1["baudRate"] = ui->baudRate1ComboBox->currentText().toInt();
    
    // Convert parity from full word to single letter
    QString parity1 = ui->parity1ComboBox->currentText();
    if (parity1 == "None") comm1["parity"] = "N";
    else if (parity1 == "Even") comm1["parity"] = "E";
    else if (parity1 == "Odd") comm1["parity"] = "O";
    else if (parity1 == "Mark") comm1["parity"] = "M";
    else if (parity1 == "Space") comm1["parity"] = "S";
    else comm1["parity"] = parity1.toStdString();
    
    comm1["dataBits"] = ui->dataBits1ComboBox->currentText().toInt();
    comm1["stopBits"] = ui->stopBits1ComboBox->currentText().toDouble();
    
    // Handle STX/ETX values - convert from hex string to appropriate format
    QString stx1Text = ui->stx1LineEdit->text().trimmed();
    if (!stx1Text.isEmpty()) {
        bool ok;
        int stxValue = stx1Text.toInt(&ok, 16);
        if (ok) {
            // For small values (0-9), store as integers
            if (stxValue <= 9) {
                comm1["stx"] = stxValue;
            } else {
                // For larger values, store as hex strings
                comm1["stx"] = "0x" + QString::number(stxValue, 16).rightJustified(2, '0').toStdString();
            }
        }
    } else {
        comm1["stx"] = "";
    }
    
    QString etx1Text = ui->etx1LineEdit->text().trimmed();
    if (!etx1Text.isEmpty()) {
        bool ok;
        int etxValue = etx1Text.toInt(&ok, 16);
        if (ok) {
            // For small values (0-9), store as integers
            if (etxValue <= 9) {
                comm1["etx"] = etxValue;
            } else {
                // For larger values, store as hex strings
                comm1["etx"] = "0x" + QString::number(etxValue, 16).rightJustified(2, '0').toStdString();
            }
        }
    } else {
        comm1["etx"] = "";
    }
    
    // Save trigger
    comm1["trigger"] = ui->commuication1TriggerLineEdit->text().toStdString();
    
    // Update communication2 settings
    nlohmann::json comm2 = nlohmann::json::object();
    comm2["port"] = ui->portName2ComboBox->currentText().toStdString();
    comm2["description"] = "reader2"; // Default description
    comm2["baudRate"] = ui->baudRate2ComboBox->currentText().toInt();
    
    // Convert parity from full word to single letter
    QString parity2 = ui->parity2ComboBox->currentText();
    if (parity2 == "None") comm2["parity"] = "N";
    else if (parity2 == "Even") comm2["parity"] = "E";
    else if (parity2 == "Odd") comm2["parity"] = "O";
    else if (parity2 == "Mark") comm2["parity"] = "M";
    else if (parity2 == "Space") comm2["parity"] = "S";
    else comm2["parity"] = parity2.toStdString();
    
    comm2["dataBits"] = ui->dataBits2ComboBox->currentText().toInt();
    comm2["stopBits"] = ui->stopBits2ComboBox->currentText().toDouble();
    
    // Handle STX/ETX values - convert from hex string to appropriate format
    QString stx2Text = ui->stx2LineEdit->text().trimmed();
    if (!stx2Text.isEmpty()) {
        bool ok;
        int stxValue = stx2Text.toInt(&ok, 16);
        if (ok) {
            // For small values (0-9), store as integers
            if (stxValue <= 9) {
                comm2["stx"] = stxValue;
            } else {
                // For larger values, store as hex strings
                comm2["stx"] = "0x" + QString::number(stxValue, 16).rightJustified(2, '0').toStdString();
            }
        }
    } else {
        comm2["stx"] = "";
    }
    
    QString etx2Text = ui->etx2LineEdit->text().trimmed();
    if (!etx2Text.isEmpty()) {
        bool ok;
        int etxValue = etx2Text.toInt(&ok, 16);
        if (ok) {
            // For small values (0-9), store as integers
            if (etxValue <= 9) {
                comm2["etx"] = etxValue;
            } else {
                // For larger values, store as hex strings
                comm2["etx"] = "0x" + QString::number(etxValue, 16).rightJustified(2, '0').toStdString();
            }
        }
    } else {
        comm2["etx"] = "";
    }
    
    // Save trigger
    comm2["trigger"] = ui->commuication2TriggerLineEdit->text().toStdString();
    
    // Update the communication settings in the commSettings object
    commSettings["communication1"] = comm1;
    commSettings["communication2"] = comm2;
    
    // Create a new JSON object for timer settings
    nlohmann::json timerSettings = nlohmann::json::object();
    
    // Get timer settings from the table
    for (int row = 0; row < ui->timersTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = ui->timersTable->item(row, 0);
        QTableWidgetItem* durationItem = ui->timersTable->item(row, 1);
        QTableWidgetItem* descriptionItem = ui->timersTable->item(row, 2);
        
        if (nameItem && durationItem && descriptionItem) {
            std::string timerName = nameItem->text().toStdString();
            int duration = durationItem->text().toInt();
            std::string description = descriptionItem->text().toStdString();
            
            // Create timer object
            nlohmann::json timerObject = nlohmann::json::object();
            timerObject["duration"] = duration;
            timerObject["description"] = description;
            
            // Add to timer settings
            timerSettings[timerName] = timerObject;
        }
    }
    
    // Update the Config object with the new settings
    try {
        // Update the communication settings
        mutableConfig->updateCommunicationSettings(commSettings);
        
        // Update the timer settings
        mutableConfig->updateTimerSettings(timerSettings);
        
        // Save the configuration to file
        if (mutableConfig->saveToFile()) {
            qDebug() << "Settings saved to configuration file";
            return true;
        } else {
            qDebug() << "Failed to save settings to configuration file";
            return false;
        }
    } catch (const std::exception& e) {
        qDebug() << "Exception while saving settings: " << e.what();
        return false;
    }
}

void SettingsWindow::markAsChanged(QWidget* widget) {
    // Set light red background to indicate a changed value
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Base, QColor(255, 200, 200)); // Light red
    widget->setPalette(palette);
}

void SettingsWindow::resetChangedFields() {
    // Reset background color for all editable fields
    
    // Communication tab fields
    QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : comboBoxes) {
        comboBox->setPalette(QPalette());
    }
    
    QList<QLineEdit*> lineEdits = findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits) {
        lineEdit->setPalette(QPalette());
    }
    
    // Timer table cells
    for (int row = 0; row < ui->timersTable->rowCount(); ++row) {
        QTableWidgetItem* durationItem = ui->timersTable->item(row, 1);
        if (durationItem) {
            // Clear the background color completely
            durationItem->setBackground(QBrush(Qt::transparent));
        }
    }
}

void SettingsWindow::connectChangeEvents() {
    // Connect change events for ComboBoxes
    QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : comboBoxes) {
        connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, comboBox](int) {
            markAsChanged(comboBox);
        });
    }
    
    // Connect change events for LineEdits
    QList<QLineEdit*> lineEdits = findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits) {
        connect(lineEdit, &QLineEdit::textChanged, [this, lineEdit](const QString&) {
            markAsChanged(lineEdit);
        });
    }
    
    // Connect change events for timer duration cells
    connect(ui->timersTable, &QTableWidget::itemChanged, [this](QTableWidgetItem* item) {
        // Only mark duration column (column 1) as changed and only if not refreshing
        if (item && item->column() == 1 && !isRefreshing_) {
            item->setBackground(QBrush(QColor(255, 200, 200))); // Light red
        }
    });
}
