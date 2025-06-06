// SettingsWindow.cpp
// Implementation of the SettingsWindow class for MachineController


// --- Project Includes ---
#include "gui/SettingsWindow.h"
#include "ui_SettingsWindow.h"
#include "Config.h"
#include "Logger.h"

// --- Qt Includes ---
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStackedWidget>
#include <QTimer>

// ============================================================================
//  UI Setup Helper Functions
// ============================================================================

// Populate the Data File tab with settings from config
void SettingsWindow::fillDataFileTabFields() {
    if (!config_) return;
    auto settings = config_->getDataFileSettings();
    if (ui->startPositionSpinBox)
        ui->startPositionSpinBox->setValue(settings.startPosition);
    if (ui->endPositionSpinBox)
        ui->endPositionSpinBox->setValue(settings.endPosition);
    if (ui->sequenceCheckBox)
        ui->sequenceCheckBox->setChecked(settings.sequenceCheck);
    if (ui->existenceCheckBox)
        ui->existenceCheckBox->setChecked(settings.existenceCheck);
    if (ui->sequenceDirectionComboBox) {
        // Map legacy values to new ones for compatibility
        QString dir = QString::fromStdString(settings.sequenceDirection);
        if (dir.compare("Forward", Qt::CaseInsensitive) == 0) dir = "Up";
        else if (dir.compare("Reverse", Qt::CaseInsensitive) == 0) dir = "Down";
        int idx = ui->sequenceDirectionComboBox->findText(dir);
        if (idx >= 0)
            ui->sequenceDirectionComboBox->setCurrentIndex(idx);
    }
}

// ============================================================================
//  Constructor, Destructor, and Initialization
// ============================================================================

// ----------------------------------------------------------------------------
// SettingsWindow Constructor
// ----------------------------------------------------------------------------
SettingsWindow::SettingsWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config)
    : QDialog(parent),
      ui(new Ui::SettingsWindow),
      eventQueue_(eventQueue),
      config_(&config),
      changedWidgets_(),
      initialLoadComplete_(false)
{
    // Prevent change signals from saving during all initial UI setup
    isRefreshing_ = true;
    ui->setupUi(this);

    // Style for timers table selection
    ui->timersTable->setStyleSheet("QTableWidget::item:selected { color: black; background-color: #c0c0ff; }");

    // Setup communication selector and defaults button
    QComboBox* commSelector = findChild<QComboBox*>("communicationSelectorComboBox");
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (commSelector && commStack) {
        connect(commSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsWindow::onCommunicationSelectorChanged);
        QPushButton* commDefaultsButton = new QPushButton("Set Defaults", this);
        commDefaultsButton->setToolTip("Reset the current communication settings to defaults");
        QHBoxLayout* selectorLayout = findChild<QHBoxLayout*>("communicationSelectorLayout");
        if (selectorLayout) {
            selectorLayout->addWidget(commDefaultsButton);
            connect(commDefaultsButton, &QPushButton::clicked, this, &SettingsWindow::onCommunicationDefaultsButtonClicked);
        }
    }

    // Setup timers defaults button
    QPushButton* timersDefaultsButton = new QPushButton("Set Defaults", this);
    timersDefaultsButton->setToolTip("Reset timer settings to defaults");
    QVBoxLayout* timersLayout = ui->timersTab->findChild<QVBoxLayout*>();
    if (timersLayout) {
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        buttonLayout->addWidget(timersDefaultsButton);
        timersLayout->insertLayout(0, buttonLayout);
        connect(timersDefaultsButton, &QPushButton::clicked, this, &SettingsWindow::onTimersDefaultsButtonClicked);
    }

    // Fill all tab fields with current config values
    fillCommunicationTabFields();
    fillTimersTabFields();
    fillIOTabFields();
    fillDataFileTabFields();

    // Connect change signals/slots
    connectChangeEvents();

    // Timers table: auto-save on duration edit
    connect(ui->timersTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        // Only handle duration column (column 1) and only if not refreshing
        if (item && item->column() == 1 && !isRefreshing_) {
            saveSettingsToConfig();
            item->setForeground(QBrush(Qt::black));
        }
    });
    isRefreshing_ = false;
    resetChangedFields();

    // Ensure communication type visibility is correct after setup
    QMetaObject::invokeMethod(this, [this]() { updateCommunicationTypeVisibility(-1); }, Qt::QueuedConnection);
    QWidget* commPage = commStack->widget(0);
    if (commPage) {
        QComboBox* typeComboBox = commPage->findChild<QComboBox*>("communicationTypeComboBox");
        if (typeComboBox) {
            connect(typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &SettingsWindow::updateCommunicationTypeVisibility);
        }
        // CommunicationActiveCheckBox and SendPushButton handled by Qt auto-connect
    }

    isRefreshing_ = false;
}
// ----------------------------------------------------------------------------
// SettingsWindow Destructor
// ----------------------------------------------------------------------------
SettingsWindow::~SettingsWindow() {
    delete ui;
}

// Removed redundant loadSettingsFromJson. Config handles file absence and defaults internally.
// Directly call fillTimersTabFields() and fillDataFileTabFields() in initialization or refresh logic as needed.


// Populate the Communication tab with settings from config
void SettingsWindow::fillCommunicationTabFields() {

    QComboBox* commSelector = findChild<QComboBox*>("communicationSelectorComboBox");
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    
    if (!commSelector || !commStack) {
        getLogger()->warn("[fillCommunicationTabFields] Communication selector or stack widget not found");
        return;
    }
    

    commSelector->clear();
    

    nlohmann::json commSettingsList = config_->getCommunicationSettings();
    

    QWidget* commPage = commStack->widget(0);
    if (!commPage) {
        getLogger()->warn("[fillCommunicationTabFields] Communication page not found");
        return;
    }
    

    QStringList portNames = {"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", 
                            "COM9", "COM10", "COM11", "COM12", "COM13", "COM14", "COM15", "COM16"};
    

    QComboBox* portNameComboBox = commPage->findChild<QComboBox*>("portNameComboBox");
    if (portNameComboBox) {
        portNameComboBox->clear();
        portNameComboBox->addItems(portNames);
    }
    

    QComboBox* baudRateComboBox = commPage->findChild<QComboBox*>("baudRateComboBox");
    if (baudRateComboBox) {
        baudRateComboBox->clear();
        baudRateComboBox->addItems({"9600", "19200", "38400", "57600", "115200"});
    }
    

    QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
    if (parityComboBox) {
        parityComboBox->clear();
        parityComboBox->addItems({"None", "Even", "Odd", "Mark", "Space"});
    }
    

    QComboBox* dataBitsComboBox = commPage->findChild<QComboBox*>("dataBitsComboBox");
    if (dataBitsComboBox) {
        dataBitsComboBox->clear();
        dataBitsComboBox->addItems({"5", "6", "7", "8"});
    }
    

    QComboBox* stopBitsComboBox = commPage->findChild<QComboBox*>("stopBitsComboBox");
    if (stopBitsComboBox) {
        stopBitsComboBox->clear();
        stopBitsComboBox->addItems({"1", "1.5", "2"});
    }
    

    for (auto& [name, settings] : commSettingsList.items()) {
        QString displayName = QString::fromStdString(name);
        
        // Add description if available
        if (settings.contains("description")) {
            QString description = QString::fromStdString(settings["description"]);
            displayName += " (" + description + ")";
        }
        
        commSelector->addItem(displayName);
    }
    

    if (commSelector->count() == 0) {
        commSelector->addItem("communication1 (Default)");
        

        QCheckBox* activeCheckBox = commPage->findChild<QCheckBox*>("communicationActiveCheckBox");
        if (activeCheckBox) {
            activeCheckBox->setChecked(true);
        }
        
        QComboBox* typeComboBox = commPage->findChild<QComboBox*>("communicationTypeComboBox");
        if (typeComboBox) {
            typeComboBox->setCurrentText("RS232");
        }
        
        if (portNameComboBox && !portNames.isEmpty()) {
            portNameComboBox->setCurrentText(portNames.first());
        }
        
        if (baudRateComboBox) {
            baudRateComboBox->setCurrentText("115200");
        }
        
        if (parityComboBox) {
            parityComboBox->setCurrentText("None");
        }
        
        if (dataBitsComboBox) {
            dataBitsComboBox->setCurrentText("8");
        }
        
        if (stopBitsComboBox) {
            stopBitsComboBox->setCurrentText("1");
        }
    }
    

    if (commSelector->count() > 0) {
        commSelector->setCurrentIndex(0);
        // This will trigger onCommunicationSelectorChanged which will update the UI
    }
    

    if (!config_) {
        getLogger()->warn("[fillCommunicationTabFields] Config object is null. Using default values.");
        fillWithDefaults();
        GuiEvent event;
        event.keyword = "GuiMessage";
        event.data = "Config object is null. Using default values.";
        event.target = "warning";
        eventQueue_.push(event);
        return;
    }
    

    auto commSettingsJson = config_->getCommunicationSettings();
    

    if (commSettingsJson.contains("communication1")) {
        auto comm1 = commSettingsJson["communication1"];
        
        // Type
        if (comm1.contains("type")) {
            QString type = QString::fromStdString(comm1["type"]);
            QComboBox* typeComboBox = commPage->findChild<QComboBox*>("communicationTypeComboBox");
            if (typeComboBox) {
                typeComboBox->setCurrentText(type);
            }
        } else {

            QComboBox* typeComboBox = commPage->findChild<QComboBox*>("communicationTypeComboBox");
            if (typeComboBox) {
                typeComboBox->setCurrentText("RS232");
            }
        }
        
        // Active state
        if (comm1.contains("active")) {
            bool active = comm1["active"];
            QCheckBox* activeCheckBox = commPage->findChild<QCheckBox*>("communicationActiveCheckBox");
            if (activeCheckBox) {
                activeCheckBox->setChecked(active);
            }
        } else {
            // Default to active if not specified
            QCheckBox* activeCheckBox = commPage->findChild<QCheckBox*>("communicationActiveCheckBox");
            if (activeCheckBox) {
                activeCheckBox->setChecked(true);
            }
        }
        
        // RS232 Settings
        if (commPage->findChild<QComboBox*>("communicationTypeComboBox")->currentText() == "RS232") {
            // Port
            if (comm1.contains("port")) {
                QString port = QString::fromStdString(comm1["port"]);
                QComboBox* portNameComboBox = commPage->findChild<QComboBox*>("portNameComboBox");
                if (portNameComboBox) {
                    portNameComboBox->setCurrentText(port);
                }
            } else if (comm1.contains("portName")) { // For backward compatibility
                QString portName = QString::fromStdString(comm1["portName"]);
                QComboBox* portNameComboBox = commPage->findChild<QComboBox*>("portNameComboBox");
                if (portNameComboBox) {
                    portNameComboBox->setCurrentText(portName);
                }
            }
        }
        
        // TCP/IP Settings
        if (commPage->findChild<QComboBox*>("communicationTypeComboBox")->currentText() == "TCP/IP") {
            if (comm1.contains("tcpip")) {
                auto tcpip = comm1["tcpip"];
                if (tcpip.contains("ip")) {
                    QString ip = QString::fromStdString(tcpip["ip"]);
                    QLineEdit* ipLineEdit = commPage->findChild<QLineEdit*>("ipLineEdit");
                    if (ipLineEdit) {
                        ipLineEdit->setText(ip);
                    }
                }
                if (tcpip.contains("port")) {
                    int port = tcpip["port"];
                    QSpinBox* portSpinBox = commPage->findChild<QSpinBox*>("portSpinBox");
                    if (portSpinBox) {
                        portSpinBox->setValue(port);
                    }
                }
                if (tcpip.contains("timeout_ms")) {
                    int timeout = tcpip["timeout_ms"];
                    QSpinBox* timeoutSpinBox = commPage->findChild<QSpinBox*>("timeoutSpinBox");
                    if (timeoutSpinBox) {
                        timeoutSpinBox->setValue(timeout);
                    }
                }
            }
        }
        
        // Baud rate
        if (comm1.contains("baudRate")) {
            int baudRate = comm1["baudRate"];
            QComboBox* baudRateComboBox = commPage->findChild<QComboBox*>("baudRateComboBox");
            if (baudRateComboBox) {
                baudRateComboBox->setCurrentText(QString::number(baudRate));
            }
        }
        
        // Parity
        if (comm1.contains("parity")) {
            QString parity = QString::fromStdString(comm1["parity"]);
            // Convert from single letter to full word
            if (parity == "N") {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText("None");
                }
            } else if (parity == "E") {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText("Even");
                }
            } else if (parity == "O") {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText("Odd");
                }
            } else if (parity == "M") {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText("Mark");
                }
            } else if (parity == "S") {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText("Space");
                }
            } else {
                QComboBox* parityComboBox = commPage->findChild<QComboBox*>("parityComboBox");
                if (parityComboBox) {
                    parityComboBox->setCurrentText(parity); // Direct setting if already in full word format
                }
            }
        }
        
        // Data bits
        if (comm1.contains("dataBits")) {
            int dataBits = comm1["dataBits"];
            QComboBox* dataBitsComboBox = commPage->findChild<QComboBox*>("dataBitsComboBox");
            if (dataBitsComboBox) {
                dataBitsComboBox->setCurrentText(QString::number(dataBits));
            }
        }
        
        // Stop bits
        if (comm1.contains("stopBits")) {
            double stopBits = comm1["stopBits"];
            QComboBox* stopBitsComboBox = commPage->findChild<QComboBox*>("stopBitsComboBox");
            if (stopBitsComboBox) {
                stopBitsComboBox->setCurrentText(QString::number(stopBits, 'g', 2));
            }
        }
        
        // STX/ETX values
        if (comm1.contains("stx")) {
            // Handle different formats (int, string, hex string)
            if (comm1["stx"].is_number()) {
                int stx = comm1["stx"];
                QLineEdit* stxLineEdit = commPage->findChild<QLineEdit*>("stxLineEdit");
                if (stxLineEdit) {
                    stxLineEdit->setText(QString::number(stx, 16).rightJustified(2, '0'));
                }
            } else if (comm1["stx"].is_string()) {
                std::string stxStr = comm1["stx"];
                // Check if it's a hex string (starts with 0x)
                if (stxStr.substr(0, 2) == "0x") {
                    // Convert from hex string to integer and then to formatted hex
                    int stxValue = std::stoi(stxStr.substr(2), nullptr, 16);
                    QLineEdit* stxLineEdit = commPage->findChild<QLineEdit*>("stxLineEdit");
                    if (stxLineEdit) {
                        stxLineEdit->setText(QString::number(stxValue, 16).rightJustified(2, '0'));
                    }
                } else {
                    // Just use the string as is
                    QLineEdit* stxLineEdit = commPage->findChild<QLineEdit*>("stxLineEdit");
                    if (stxLineEdit) {
                        stxLineEdit->setText(QString::fromStdString(stxStr));
                    }
                }
            }
        }
        
        if (comm1.contains("etx")) {
            // Handle different formats (int, string, hex string)
            if (comm1["etx"].is_number()) {
                int etx = comm1["etx"];
                QLineEdit* etxLineEdit = commPage->findChild<QLineEdit*>("etxLineEdit");
                if (etxLineEdit) {
                    etxLineEdit->setText(QString::number(etx, 16).rightJustified(2, '0'));
                }
            } else if (comm1["etx"].is_string()) {
                std::string etxStr = comm1["etx"];
                // Check if it's a hex string (starts with 0x)
                if (etxStr.substr(0, 2) == "0x") {
                    // Convert from hex string to integer and then to formatted hex
                    int etxValue = std::stoi(etxStr.substr(2), nullptr, 16);
                    QLineEdit* etxLineEdit = commPage->findChild<QLineEdit*>("etxLineEdit");
                    if (etxLineEdit) {
                        etxLineEdit->setText(QString::number(etxValue, 16).rightJustified(2, '0'));
                    }
                } else {
                    // Just use the string as is
                    QLineEdit* etxLineEdit = commPage->findChild<QLineEdit*>("etxLineEdit");
                    if (etxLineEdit) {
                        etxLineEdit->setText(QString::fromStdString(etxStr));
                    }
                }
            }
        }
        
        // Set test message
        if (comm1.contains("trigger")) {
            QString trigger = QString::fromStdString(comm1["trigger"]);
            QLineEdit* triggerLineEdit = commPage->findChild<QLineEdit*>("communicationTriggerLineEdit");
            if (triggerLineEdit) {
                triggerLineEdit->setText(trigger);
            }
        }
        
        // No more code needed here
        
        // No more code needed here
    }
    
    // No more code needed here
    

    
    // Set defaults for STX/ETX if not already set
    QLineEdit* stxLineEdit = findChild<QLineEdit*>("stxLineEdit");
    if (stxLineEdit && stxLineEdit->text().isEmpty()) {
        stxLineEdit->setText("02"); // Default if not specified
    }
    
    QLineEdit* etxLineEdit = findChild<QLineEdit*>("etxLineEdit");
    if (etxLineEdit && etxLineEdit->text().isEmpty()) {
        etxLineEdit->setText("03"); // Default if not specified
    }
    
    // Set trigger default only if it's not already set
    QLineEdit* triggerLineEdit = findChild<QLineEdit*>("communicationTriggerLineEdit");
    if (triggerLineEdit && triggerLineEdit->text().isEmpty()) {
        triggerLineEdit->setText("t");
    }
    
    // Emit a message to the event queue that settings were loaded
    GuiEvent commLoadedEvent;
    commLoadedEvent.keyword = "GuiMessage";
    commLoadedEvent.data = "Communication settings loaded from JSON";
    commLoadedEvent.target = "info";
    eventQueue_.push(commLoadedEvent);
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

// Reset the current communication channel settings to their defaults
void SettingsWindow::onCommunicationDefaultsButtonClicked() {
    getLogger()->debug("Communication defaults button clicked");
    
    // Set the refreshing flag to prevent marking fields as changed during refresh
    isRefreshing_ = true;
    
    // Get the current communication name
    if (!currentCommunicationName_.empty()) {
        // Set default values for the current communication channel
        QComboBox* portNameComboBox = findChild<QComboBox*>("portNameComboBox");
        if (portNameComboBox) {
            portNameComboBox->setCurrentText("COM1");
        }
        
        QComboBox* baudRateComboBox = findChild<QComboBox*>("baudRateComboBox");
        if (baudRateComboBox) {
            baudRateComboBox->setCurrentText("115200");
        }
        
        QComboBox* parityComboBox = findChild<QComboBox*>("parityComboBox");
        if (parityComboBox) {
            parityComboBox->setCurrentText("None");
        }
        
        QComboBox* dataBitsComboBox = findChild<QComboBox*>("dataBitsComboBox");
        if (dataBitsComboBox) {
            dataBitsComboBox->setCurrentText("8");
        }
        
        QComboBox* stopBitsComboBox = findChild<QComboBox*>("stopBitsComboBox");
        if (stopBitsComboBox) {
            stopBitsComboBox->setCurrentText("1");
        }
        
        QLineEdit* stxLineEdit = findChild<QLineEdit*>("stxLineEdit");
        if (stxLineEdit) {
            stxLineEdit->setText("02");
        }
        
        QLineEdit* etxLineEdit = findChild<QLineEdit*>("etxLineEdit");
        if (etxLineEdit) {
            etxLineEdit->setText("03");
        }
        
        QLineEdit* triggerLineEdit = findChild<QLineEdit*>("communicationTriggerLineEdit");
        if (triggerLineEdit) {
            triggerLineEdit->setText("t");
        }
        
        // Set the communication type to RS232 by default
        QComboBox* commTypeComboBox = findChild<QComboBox*>("communicationTypeComboBox");
        if (commTypeComboBox) {
            commTypeComboBox->setCurrentText("RS232");
        }
        
        // Set active checkbox to checked by default
        QCheckBox* activeCheckBox = findChild<QCheckBox*>("communicationActiveCheckBox");
        if (activeCheckBox) {
            activeCheckBox->setChecked(true);
        }
        
        // Save the default settings
        saveSettingsToConfig();
        
        // Notify user
        GuiEvent successEvent;
        successEvent.keyword = "GuiMessage";
        successEvent.data = "Communication settings for " + currentCommunicationName_ + " reset to defaults";
        successEvent.target = "info";
        eventQueue_.push(successEvent);
    }
    
    // Reset the refreshing flag
    isRefreshing_ = false;
}

// Reset all timer settings to their defaults
void SettingsWindow::onTimersDefaultsButtonClicked() {
    getLogger()->debug("Timers defaults button clicked");
    
    // Set the refreshing flag to prevent marking fields as changed during refresh
    isRefreshing_ = true;
    
    // Clear the timers table
    ui->timersTable->setRowCount(0);
    
    // Add default timers
    // Timer 1
    int row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem1 = new QTableWidgetItem("timer1");
    QTableWidgetItem* durationItem1 = new QTableWidgetItem("1000");
    QTableWidgetItem* descriptionItem1 = new QTableWidgetItem("General purpose timer 1");
    nameItem1->setFlags(nameItem1->flags() & ~Qt::ItemIsEditable);
    descriptionItem1->setFlags(descriptionItem1->flags() & ~Qt::ItemIsEditable);
    ui->timersTable->setItem(row, 0, nameItem1);
    ui->timersTable->setItem(row, 1, durationItem1);
    ui->timersTable->setItem(row, 2, descriptionItem1);
    
    // Timer 2
    row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem2 = new QTableWidgetItem("timer2");
    QTableWidgetItem* durationItem2 = new QTableWidgetItem("2000");
    QTableWidgetItem* descriptionItem2 = new QTableWidgetItem("General purpose timer 2");
    nameItem2->setFlags(nameItem2->flags() & ~Qt::ItemIsEditable);
    descriptionItem2->setFlags(descriptionItem2->flags() & ~Qt::ItemIsEditable);
    ui->timersTable->setItem(row, 0, nameItem2);
    ui->timersTable->setItem(row, 1, durationItem2);
    ui->timersTable->setItem(row, 2, descriptionItem2);
    
    // Timer 3
    row = ui->timersTable->rowCount();
    ui->timersTable->insertRow(row);
    QTableWidgetItem* nameItem3 = new QTableWidgetItem("timer3");
    QTableWidgetItem* durationItem3 = new QTableWidgetItem("5000");
    QTableWidgetItem* descriptionItem3 = new QTableWidgetItem("General purpose timer 3");
    nameItem3->setFlags(nameItem3->flags() & ~Qt::ItemIsEditable);
    descriptionItem3->setFlags(descriptionItem3->flags() & ~Qt::ItemIsEditable);
    ui->timersTable->setItem(row, 0, nameItem3);
    ui->timersTable->setItem(row, 1, durationItem3);
    ui->timersTable->setItem(row, 2, descriptionItem3);
    
    // Save the default settings
    saveSettingsToConfig();
    
    // Notify user
    GuiEvent successEvent;
    successEvent.keyword = "GuiMessage";
    successEvent.data = "Timer settings reset to defaults";
    successEvent.target = "info";
    eventQueue_.push(successEvent);
    
    // Reset the refreshing flag
    isRefreshing_ = false;
}


void SettingsWindow::on_refreshButton_clicked()
{
    getLogger()->debug("Refresh button clicked");
    fillIOTabFields();
    GuiEvent event;
event.keyword = "GuiMessage";
}

// ============================================================================
//  Helper Methods
// ============================================================================

// Update the UI with the latest input states from the logic layer
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


// Populate the IO tab with settings from config
void SettingsWindow::fillIOTabFields() {
    if (!config_) {
        getLogger()->warn("[fillIOTabFields] Config object is null. Cannot load IO settings.");
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
                    disconnect(checkbox, &QCheckBox::checkStateChanged, nullptr, nullptr);
                    
                    // Connect checkbox state change to handle output overrides
                    // We use a lambda to capture the checkbox for each connection
                    // Note: Using stateChanged instead of checkStateChanged for compatibility
                    // with the existing UI connections, despite it being deprecated
                    connect(checkbox, &QCheckBox::checkStateChanged, [this, checkbox](int state) {
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
        getLogger()->warn("[fillIOTabFields] Exception while loading IO settings: {}", e.what());
    }
}

// Collect and send current output states to Logic
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
// Handle individual output checkbox state changes
void SettingsWindow::handleOutputCheckboxStateChanged(const QString& outputName, int state) {
    getLogger()->debug("Output override for {}: {}", outputName.toStdString(), (state == Qt::Checked ? "ON" : "OFF"));
    
    // Send updated output states to Logic
    sendCurrentOutputStates();
}

// Populate the Timers tab with settings from config
void SettingsWindow::fillTimersTabFields()
{
    isRefreshing_ = true;
    ui->timersTable->blockSignals(true);
    if (!config_) {
        getLogger()->warn("[fillTimersTabFields] Config object is null. Cannot load timer settings.");
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
            
            // Ensure all items have proper colors for good visibility
            nameItem->setForeground(QBrush(Qt::black));
            durationItem->setBackground(QBrush(Qt::transparent));
            durationItem->setForeground(QBrush(Qt::black));
            descriptionItem->setForeground(QBrush(Qt::black));
            
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
        getLogger()->warn("[fillTimersTabFields] Exception while loading timer settings: {}", e.what());
    }
    ui->timersTable->blockSignals(false);
    isRefreshing_ = false;

}

// Fill all tabs with default values (used when config is missing or invalid)
void SettingsWindow::fillWithDefaults()
{
    // Fill Communication settings with default values
    QComboBox* commSelector = findChild<QComboBox*>("communicationSelectorComboBox");
    if (commSelector) {
        // Make sure we have at least one communication channel
        if (commSelector->count() == 0) {
            commSelector->addItem("Communication 1");
        }
        commSelector->setCurrentIndex(0);
    }
    
    QComboBox* portNameComboBox = findChild<QComboBox*>("portNameComboBox");
    if (portNameComboBox) {
        portNameComboBox->setCurrentText("COM1");
    }
    
    QComboBox* baudRateComboBox = findChild<QComboBox*>("baudRateComboBox");
    if (baudRateComboBox) {
        baudRateComboBox->setCurrentText("115200");
    }
    
    QComboBox* parityComboBox = findChild<QComboBox*>("parityComboBox");
    if (parityComboBox) {
        parityComboBox->setCurrentText("None");
    }
    
    QComboBox* dataBitsComboBox = findChild<QComboBox*>("dataBitsComboBox");
    if (dataBitsComboBox) {
        dataBitsComboBox->setCurrentText("8");
    }
    
    QComboBox* stopBitsComboBox = findChild<QComboBox*>("stopBitsComboBox");
    if (stopBitsComboBox) {
        stopBitsComboBox->setCurrentText("1");
    }
    
    QLineEdit* stxLineEdit = findChild<QLineEdit*>("stxLineEdit");
    if (stxLineEdit) {
        stxLineEdit->setText("02");
    }
    
    QLineEdit* etxLineEdit = findChild<QLineEdit*>("etxLineEdit");
    if (etxLineEdit) {
        etxLineEdit->setText("03");
    }
    
    QLineEdit* triggerLineEdit = findChild<QLineEdit*>("communicationTriggerLineEdit");
    if (triggerLineEdit) {
        triggerLineEdit->setText("t");
    }
    
    // Set the communication type to RS232 by default
    QComboBox* commTypeComboBox = findChild<QComboBox*>("communicationTypeComboBox");
    if (commTypeComboBox) {
        commTypeComboBox->setCurrentText("RS232");
    }
    
    // Set active checkbox to checked by default
    QCheckBox* activeCheckBox = findChild<QCheckBox*>("communicationActiveCheckBox");
    if (activeCheckBox) {
        activeCheckBox->setChecked(true);
    }
    
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
    GuiEvent event;
event.keyword = "GuiMessage";
event.data = "Default communication settings loaded";
event.target = "settings";
eventQueue_.push(event);
    
    // Prevent marking items as changed during refresh
    isRefreshing_ = true;
}

// Helper to parse char settings (STX, ETX) from int, hex string, or char string
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
            } catch (const std::exception &e) {
                getLogger()->warn("[parseCharSetting] Invalid hex value for '{}': {}. Exception: {}", key, strValue, e.what());
                return defaultValue;
            }
        }
        else {
            return strValue[0]; // Take the first character.
        }
    }
    else {
        getLogger()->warn("[parseCharSetting] Invalid type for '{}' setting. Using default value.", key);
        return defaultValue;
    }
}

// ============================================================================
//  Settings Management Functions
// ============================================================================

// Save all settings from the UI to the config and persist to file
bool SettingsWindow::saveSettingsToConfig() {
    if (!config_) {
        getLogger()->warn("[saveSettingsToConfig] Config object is null. Cannot save settings.");
        return false;
    }
    
    // We need to cast away the const-ness of config_ to modify it
    // This is safe because we know the Config object is owned by MainWindow and outlives SettingsWindow
    Config* mutableConfig = const_cast<Config*>(config_);
    getLogger()->info("[saveSettingsToConfig] Saving settings to config");
    // Create a new JSON object for communication settings
    nlohmann::json commSettings = nlohmann::json::object();
    
    // We'll use the saveCurrentCommunicationSettings method to save the current communication settings
    // This will ensure that all settings are saved properly
    saveCurrentCommunicationSettings();
    
    // Update timer settings via fine-grained setters
    // Gather all timer settings from the UI and update the config
    nlohmann::json timersJson = nlohmann::json::object();
    if (ui->timersTable->rowCount() > 0) {
        for (int row = 0; row < ui->timersTable->rowCount(); ++row) {
            QTableWidgetItem* nameItem = ui->timersTable->item(row, 0);
            QTableWidgetItem* durationItem = ui->timersTable->item(row, 1);
            QTableWidgetItem* descriptionItem = ui->timersTable->item(row, 2);
            if (nameItem && durationItem && descriptionItem) {
                std::string timerName = nameItem->text().toStdString();
                int duration = durationItem->text().toInt();
                std::string description = descriptionItem->text().toStdString();
                timersJson[timerName] = {
                    {"duration", duration},
                    {"description", description}
                };
            }
        }
        mutableConfig->updateTimerSettings(timersJson);
        // Send a ParameterChange event to notify Logic to reinitialize affected components
        GuiEvent paramEvent;
        paramEvent.keyword = "ParameterChange";
        paramEvent.target = "timer";
        eventQueue_.push(paramEvent);
    }
    
    // --- Save Data File Tab Values ---
    Config::DataFileSettings dataFileSettings;
    if (ui->startPositionSpinBox)
        dataFileSettings.startPosition = ui->startPositionSpinBox->value();
    if (ui->endPositionSpinBox)
        dataFileSettings.endPosition = ui->endPositionSpinBox->value();
    if (ui->sequenceCheckBox)
        dataFileSettings.sequenceCheck = ui->sequenceCheckBox->isChecked();
    if (ui->existenceCheckBox)
        dataFileSettings.existenceCheck = ui->existenceCheckBox->isChecked();
    if (ui->sequenceDirectionComboBox)
        dataFileSettings.sequenceDirection = ui->sequenceDirectionComboBox->currentText().toStdString();
    
    mutableConfig->setDataFileSettings(dataFileSettings);
    // todo : update logic


    // Update the Config object with the new settings
    try {
        // Timer settings are now updated via setTimerSetting above; no need to call updateTimerSettings here.
        
        // Save the configuration to file
        if (mutableConfig->saveToFile()) {
            getLogger()->debug("Settings saved to configuration file");
            
            return true;
        } else {
            getLogger()->warn("[saveSettingsToConfig] Failed to save settings to configuration file");
            return false;
        }
    } catch (const std::exception& e) {
        getLogger()->warn("[saveSettingsToConfig] Exception while saving settings: {}", e.what());
        return false;
    }
}

// ============================================================================
//  UI Event Handler Functions
// ============================================================================

// Mark a widget as changed (legacy, now a no-op)
void SettingsWindow::markAsChanged(QWidget* widget) {
    // This method is kept for backward compatibility but no longer changes the widget appearance
    // since we're saving changes immediately
    Q_UNUSED(widget);
}

// Reset all changed field markers (legacy, now a no-op)
void SettingsWindow::resetChangedFields() {
    // This method is kept for backward compatibility but no longer does anything
    // since we're saving changes immediately and not marking fields as changed
}

// Connect UI change events to save logic
void SettingsWindow::connectChangeEvents() {
    // Connect change events for all combo boxes
    QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : comboBoxes) {
        // Skip the communication selector dropdown - we handle it separately
        if (comboBox->objectName() == "communicationSelectorComboBox") {
            continue;
        }
        
        connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, comboBox](int) {
            // Skip if we're refreshing the UI
            if (isRefreshing_) return;
            
            // Save changes immediately
            // saveCurrentCommunicationSettings() is called inside saveSettingsToConfig()
            saveSettingsToConfig();
        });
    }
    
    // Connect change events for LineEdits
    QList<QLineEdit*> lineEdits = findChildren<QLineEdit*>();
    for (QLineEdit* lineEdit : lineEdits) {
        connect(lineEdit, &QLineEdit::textChanged, [this, lineEdit](const QString&) {
            // Skip if we're refreshing the UI
            if (isRefreshing_) return;
            
            // Save changes immediately
            saveSettingsToConfig();
        });
    }
    
    // Connect change events for CheckBoxes
    QList<QCheckBox*> checkBoxes = findChildren<QCheckBox*>();
    for (QCheckBox* checkBox : checkBoxes) {
        // Skip output override checkboxes which are handled separately
        if (checkBox->objectName().contains("outputOverride")) {
            continue;
        }
        
        connect(checkBox, &QCheckBox::checkStateChanged, [this, checkBox](int) {
            // Skip if we're refreshing the UI
            if (isRefreshing_) return;
            
            // Save changes immediately
            saveSettingsToConfig();
        });
    }
    
    // Connect change events for SpinBoxes
    QList<QSpinBox*> spinBoxes = findChildren<QSpinBox*>();
    for (QSpinBox* spinBox : spinBoxes) {
        connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this, spinBox](int) {
            // Skip if we're refreshing the UI
            if (isRefreshing_) return;
            
            // Save changes immediately
            saveSettingsToConfig();
        });
    }
    
    // Note: Timer table itemChanged signal is connected in fillTimersTabFields
    // to properly handle disconnection/reconnection during initialization
}

void SettingsWindow::on_communicationSendPushButton_clicked()
{
    // Get the current page
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (!commStack) {
        return;
    }
    
    QWidget* currentPage = commStack->currentWidget();
    if (!currentPage) {
        return;
    }
    
    // Find the trigger line edit in the current page
    QLineEdit* triggerLineEdit = currentPage->findChild<QLineEdit*>("communicationTriggerLineEdit");
    if (!triggerLineEdit) {
        getLogger()->warn("[on_communicationSendPushButton_clicked] Cannot find trigger line edit");
        return;
    }
    
    QString message = triggerLineEdit->text();
    if (message.isEmpty()) {
        getLogger()->warn("[on_communicationSendPushButton_clicked] Cannot send empty message to {}", currentCommunicationName_);
        return;
    }
    
    // Send the message using the current communication channel
    GuiEvent event;
    event.keyword = "SendCommunicationMessage";
    event.data = message.toStdString();
    event.target = currentCommunicationName_;
    eventQueue_.push(event);
    
    getLogger()->debug("Sent message to {}: {}", currentCommunicationName_, message.toStdString());
}

void SettingsWindow::updateCommunicationTypeVisibility(int index) {
    // Get the stacked widget to determine which communication is currently visible
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (!commStack) {
        return; // Can't find the stacked widget, nothing to update
    }
    
    // Get the current page
    QWidget* currentPage = commStack->currentWidget();
    if (!currentPage) {
        return; // No current page
    }
    
    // Find the communication type combo box in the current page
    QComboBox* typeComboBox = currentPage->findChild<QComboBox*>("communicationTypeComboBox");
    if (!typeComboBox) {
        return; // Can't find the type combo box
    }
    
    // Get the current communication type
    QString commType = typeComboBox->currentText();
    
    // Find the RS232 and TCP/IP group boxes
    QGroupBox* rs232Group = currentPage->findChild<QGroupBox*>("rs232Group");
    QGroupBox* tcpipGroup = currentPage->findChild<QGroupBox*>("tcpipGroup");
    
    // Show/hide the appropriate group box based on the communication type
    if (rs232Group) {
        rs232Group->setVisible(commType == "RS232");
    }
    
    if (tcpipGroup) {
        tcpipGroup->setVisible(commType == "TCP/IP");
    }
}

void SettingsWindow::on_communicationActiveCheckBox_stateChanged(int state) {
    // Enable/disable the communication settings based on the active state
    bool isActive = (state == Qt::Checked);
    
    // Mark the checkbox as changed
    QCheckBox* activeCheckBox = qobject_cast<QCheckBox*>(sender());
    if (activeCheckBox) {
        markAsChanged(activeCheckBox);
    }
    
    // Get the current page
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (!commStack) {
        return;
    }
    
    QWidget* currentPage = commStack->currentWidget();
    if (!currentPage) {
        return;
    }
    
    // Find the UI elements in the current page
    QComboBox* typeComboBox = currentPage->findChild<QComboBox*>("communicationTypeComboBox");
    QGroupBox* rs232Group = currentPage->findChild<QGroupBox*>("rs232Group");
    QGroupBox* tcpipGroup = currentPage->findChild<QGroupBox*>("tcpipGroup");
    
    // Enable/disable the type combo box
    if (typeComboBox) {
        typeComboBox->setEnabled(isActive);
    }
    
    // Enable/disable all groups based on active state
    if (rs232Group) rs232Group->setEnabled(isActive);
    if (tcpipGroup) rs232Group->setEnabled(isActive);
}

void SettingsWindow::onCommunicationSelectorChanged(int index) {
    QComboBox* commSelector = findChild<QComboBox*>("communicationSelectorComboBox");
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    
    if (!commSelector || !commStack || index < 0 || index >= commSelector->count()) {
        return;
    }
    
    // First, save the current communication settings if we have a valid current selection
    // but don't trigger a ParameterChange event since we're just switching views
    if (!currentCommunicationName_.empty()) {
        // Save the current settings to the internal structure only
        // We're just switching between communication sets, not actually changing settings
        bool wasRefreshing = isRefreshing_;
        isRefreshing_ = true; // Set this to prevent triggering save events
        saveCurrentCommunicationSettings();
        isRefreshing_ = wasRefreshing; // Restore the previous state
    }
    
    // Update the stacked widget to show the selected page
    commStack->setCurrentIndex(0); // Always use the first page since we now have only one page
    
    // Get the communication settings from the config
    nlohmann::json commSettingsList = config_->getCommunicationSettings();
    
    // Extract the communication name from the selector text (format: "name (description)")
    QString selectorText = commSelector->itemText(index);
    QString commName = selectorText.section(" (", 0, 0);
    
    // Store the current communication name for later use
    currentCommunicationName_ = commName.toStdString();
    
    // Find the communication settings in the JSON
    if (commSettingsList.contains(currentCommunicationName_)) {
        auto& commData = commSettingsList[currentCommunicationName_];
        
        // Set the refreshing flag to prevent marking fields as changed during update
        isRefreshing_ = true;
        
        // Update the UI elements based on the selected communication channel
        // We'll use the UI elements from the current visible page in the stack
        QWidget* currentPage = commStack->currentWidget();
        
        if (currentPage) {
            // Find the UI elements in the current page using generic names
            QComboBox* typeComboBox = currentPage->findChild<QComboBox*>("communicationTypeComboBox");
            QCheckBox* activeCheckBox = currentPage->findChild<QCheckBox*>("communicationActiveCheckBox");
            QLineEdit* descriptionLineEdit = currentPage->findChild<QLineEdit*>("descriptionLineEdit");
            
            // Set the description
            if (descriptionLineEdit && commData.contains("description")) {
                descriptionLineEdit->setText(QString::fromStdString(commData["description"].get<std::string>()));
            }
            
            // Set the communication type
            if (typeComboBox && commData.contains("type")) {
                QString type = QString::fromStdString(commData["type"].get<std::string>());
                int typeIndex = typeComboBox->findText(type);
                if (typeIndex >= 0) {
                    typeComboBox->setCurrentIndex(typeIndex);
                }
            }
            
            // Set the active state
            if (activeCheckBox && commData.contains("active")) {
                activeCheckBox->setChecked(commData["active"].get<bool>());
            }
            
            // Update the RS232 settings if applicable
            if (commData.contains("type") && commData["type"].get<std::string>() == "RS232") {
                QComboBox* portNameComboBox = currentPage->findChild<QComboBox*>("portNameComboBox");
                QComboBox* baudRateComboBox = currentPage->findChild<QComboBox*>("baudRateComboBox");
                QComboBox* parityComboBox = currentPage->findChild<QComboBox*>("parityComboBox");
                QComboBox* dataBitsComboBox = currentPage->findChild<QComboBox*>("dataBitsComboBox");
                QComboBox* stopBitsComboBox = currentPage->findChild<QComboBox*>("stopBitsComboBox");
                QLineEdit* stxLineEdit = currentPage->findChild<QLineEdit*>("stxLineEdit");
                QLineEdit* etxLineEdit = currentPage->findChild<QLineEdit*>("etxLineEdit");
                QLineEdit* triggerLineEdit = currentPage->findChild<QLineEdit*>("communicationTriggerLineEdit"); // Fixed typo
                
                // Set port name
                if (portNameComboBox && commData.contains("port")) {
                    QString port = QString::fromStdString(commData["port"].get<std::string>());
                    int portIndex = portNameComboBox->findText(port);
                    if (portIndex >= 0) {
                        portNameComboBox->setCurrentIndex(portIndex);
                    }
                }
                
                // Set baud rate
                if (baudRateComboBox && commData.contains("baudRate")) {
                    QString baudRate = QString::number(commData["baudRate"].get<int>());
                    int baudIndex = baudRateComboBox->findText(baudRate);
                    if (baudIndex >= 0) {
                        baudRateComboBox->setCurrentIndex(baudIndex);
                    }
                }
                
                // Set parity
                if (parityComboBox && commData.contains("parity")) {
                    QString parity = QString::fromStdString(commData["parity"].get<std::string>());
                    // Convert from single character to full name
                    QString parityName = "None";
                    if (parity == "E" || parity == "e") parityName = "Even";
                    else if (parity == "O" || parity == "o") parityName = "Odd";
                    else if (parity == "M" || parity == "m") parityName = "Mark";
                    else if (parity == "S" || parity == "s") parityName = "Space";
                    
                    int parityIndex = parityComboBox->findText(parityName);
                    if (parityIndex >= 0) {
                        parityComboBox->setCurrentIndex(parityIndex);
                    }
                }
                
                // Set data bits
                if (dataBitsComboBox && commData.contains("dataBits")) {
                    QString dataBits = QString::number(commData["dataBits"].get<int>());
                    int dataBitsIndex = dataBitsComboBox->findText(dataBits);
                    if (dataBitsIndex >= 0) {
                        dataBitsComboBox->setCurrentIndex(dataBitsIndex);
                    }
                }
                
                // Set stop bits
                if (stopBitsComboBox && commData.contains("stopBits")) {
                    QString stopBits = QString::number(commData["stopBits"].get<double>());
                    int stopBitsIndex = stopBitsComboBox->findText(stopBits);
                    if (stopBitsIndex >= 0) {
                        stopBitsComboBox->setCurrentIndex(stopBitsIndex);
                    }
                }
                
                // Set STX/ETX values
                if (stxLineEdit && commData.contains("stx")) {
                    if (commData["stx"].is_number()) {
                        stxLineEdit->setText(QString::number(commData["stx"].get<int>()));
                    } else if (commData["stx"].is_string()) {
                        stxLineEdit->setText(QString::fromStdString(commData["stx"].get<std::string>()));
                    }
                }
                
                if (etxLineEdit && commData.contains("etx")) {
                    if (commData["etx"].is_number()) {
                        etxLineEdit->setText(QString::number(commData["etx"].get<int>()));
                    } else if (commData["etx"].is_string()) {
                        etxLineEdit->setText(QString::fromStdString(commData["etx"].get<std::string>()));
                    }
                }
                
                // Set trigger character
                if (triggerLineEdit && commData.contains("trigger")) {
                    triggerLineEdit->setText(QString::fromStdString(commData["trigger"].get<std::string>()));
                }
            }
            // Update the TCP/IP settings if applicable
            else if (commData.contains("type") && commData["type"].get<std::string>() == "TCP/IP" && commData.contains("tcpip")) {
                auto& tcpip = commData["tcpip"];
                QLineEdit* ipAddressLineEdit = currentPage->findChild<QLineEdit*>("ipLineEdit");
                QSpinBox* tcpPortSpinBox = currentPage->findChild<QSpinBox*>("portSpinBox");
                QSpinBox* timeoutSpinBox = currentPage->findChild<QSpinBox*>("timeoutSpinBox");
                
                if (ipAddressLineEdit && tcpip.contains("ip")) {
                    ipAddressLineEdit->setText(QString::fromStdString(tcpip["ip"].get<std::string>()));
                }
                
                if (tcpPortSpinBox && tcpip.contains("port")) {
                    tcpPortSpinBox->setValue(tcpip["port"].get<int>());
                }
                
                if (timeoutSpinBox && tcpip.contains("timeout_ms")) {
                    timeoutSpinBox->setValue(tcpip["timeout_ms"].get<int>());
                }
                
            }
            
            // Update the offset field
            QSpinBox* offsetSpinBox = currentPage->findChild<QSpinBox*>("offsetSpinBox");
            if (offsetSpinBox && commData.contains("offset")) {
                offsetSpinBox->setValue(commData["offset"].get<int>());
            }
        }
        
        // Call updateCommunicationTypeVisibility to ensure the correct settings are shown
        updateCommunicationTypeVisibility(-1); // Use -1 to indicate we're using the generic UI elements
        
        // Reset the refreshing flag
        isRefreshing_ = false;
    }
}

// Gather and update the current communication settings in the config
void SettingsWindow::saveCurrentCommunicationSettings() {
    if (currentCommunicationName_.empty()) {
        return; // Nothing to save
    }
    
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (!commStack) {
        return;
    }
    
    QWidget* currentPage = commStack->currentWidget();
    
    if (!currentPage) {
        return;
    }
    
    // Get the current settings from the UI
    nlohmann::json commSettings;
    
    // Get the communication type
    QComboBox* typeComboBox = currentPage->findChild<QComboBox*>("communicationTypeComboBox");
    if (typeComboBox) {
        commSettings["type"] = typeComboBox->currentText().toStdString();
    }
    
    // Get the active state
    QCheckBox* activeCheckBox = currentPage->findChild<QCheckBox*>("communicationActiveCheckBox");
    if (activeCheckBox) {
        commSettings["active"] = activeCheckBox->isChecked();
    }
    
    // Get the description
    QLineEdit* descriptionLineEdit = currentPage->findChild<QLineEdit*>("descriptionLineEdit");
    if (descriptionLineEdit) {
        commSettings["description"] = descriptionLineEdit->text().toStdString();
    }
    
    // Get RS232 settings if applicable
    if (typeComboBox && typeComboBox->currentText() == "RS232") {
        QComboBox* portNameComboBox = currentPage->findChild<QComboBox*>("portNameComboBox");
        QComboBox* baudRateComboBox = currentPage->findChild<QComboBox*>("baudRateComboBox");
        QComboBox* parityComboBox = currentPage->findChild<QComboBox*>("parityComboBox");
        QComboBox* dataBitsComboBox = currentPage->findChild<QComboBox*>("dataBitsComboBox");
        QComboBox* stopBitsComboBox = currentPage->findChild<QComboBox*>("stopBitsComboBox");
        QLineEdit* stxLineEdit = currentPage->findChild<QLineEdit*>("stxLineEdit");
        QLineEdit* etxLineEdit = currentPage->findChild<QLineEdit*>("etxLineEdit");
        QLineEdit* triggerLineEdit = currentPage->findChild<QLineEdit*>("communicationTriggerLineEdit"); // Fixed typo
        
        if (portNameComboBox) {
            commSettings["port"] = portNameComboBox->currentText().toStdString();
        }
        
        if (baudRateComboBox) {
            commSettings["baudRate"] = baudRateComboBox->currentText().toInt();
        }
        
        if (parityComboBox) {
            // Convert from full name to single character
            QString parityName = parityComboBox->currentText();
            QString parityChar = "N";
            if (parityName == "Even") parityChar = "E";
            else if (parityName == "Odd") parityChar = "O";
            else if (parityName == "Mark") parityChar = "M";
            else if (parityName == "Space") parityChar = "S";
            
            commSettings["parity"] = parityChar.toStdString();
        }
        
        if (dataBitsComboBox) {
            commSettings["dataBits"] = dataBitsComboBox->currentText().toInt();
        }
        
        if (stopBitsComboBox) {
            commSettings["stopBits"] = stopBitsComboBox->currentText().toDouble();
        }
        
        if (stxLineEdit) {
            // Try to convert to int, otherwise store as string
            bool ok;
            int stxValue = stxLineEdit->text().toInt(&ok);
            if (ok) {
                commSettings["stx"] = stxValue;
            } else {
                commSettings["stx"] = stxLineEdit->text().toStdString();
            }
        }
        
        if (etxLineEdit) {
            // Try to convert to int, otherwise store as string
            bool ok;
            int etxValue = etxLineEdit->text().toInt(&ok);
            if (ok) {
                commSettings["etx"] = etxValue;
            } else {
                commSettings["etx"] = etxLineEdit->text().toStdString();
            }
        }
        
        if (triggerLineEdit) {
            commSettings["trigger"] = triggerLineEdit->text().toStdString();
        }
    }
    // Get TCP/IP settings if applicable
    else if (typeComboBox && typeComboBox->currentText() == "TCP/IP") {
        QLineEdit* ipAddressLineEdit = currentPage->findChild<QLineEdit*>("ipLineEdit");
        QSpinBox* tcpPortSpinBox = currentPage->findChild<QSpinBox*>("portSpinBox");
        QSpinBox* timeoutSpinBox = currentPage->findChild<QSpinBox*>("timeoutSpinBox");
        
        nlohmann::json tcpipSettings;
        
        if (ipAddressLineEdit) {
            tcpipSettings["ip"] = ipAddressLineEdit->text().toStdString();
        }
        
        if (tcpPortSpinBox) {
            tcpipSettings["port"] = tcpPortSpinBox->value();
        }
        
        if (timeoutSpinBox) {
            tcpipSettings["timeout_ms"] = timeoutSpinBox->value();
        }
        
        commSettings["tcpip"] = tcpipSettings;
    }

    // Save offset (top-level, for all types)
    QSpinBox* offsetSpinBox = currentPage->findChild<QSpinBox*>("offsetSpinBox");
    if (offsetSpinBox) {
        commSettings["offset"] = offsetSpinBox->value();
    }
    
    // Update the settings in the config only if we're not in refreshing mode
    // This prevents sending events when just switching between communication channels
    if (!isRefreshing_) {
        Config* mutableConfig = const_cast<Config*>(config_);
        // Update only the current communication channel in the config JSON
        nlohmann::json allCommSettings = mutableConfig->getCommunicationSettings();
        allCommSettings[currentCommunicationName_] = commSettings;
        mutableConfig->updateCommunicationSettings(allCommSettings);
        // Only notify that settings have been updated when not in refreshing mode
        GuiEvent event;
        event.keyword = "ParameterChange";
        event.target = "communication";
        eventQueue_.push(event);
    } else {
        // We're just storing the settings in memory for later use
        // This happens when switching between communication channels
        // We don't want to trigger a save event in this case
        getLogger()->debug("Skipping config update while in refreshing mode");
    }
}

void SettingsWindow::onInitialLoadComplete()
{
    getLogger()->debug("SettingsWindow::onInitialLoadComplete() called.");
    initialLoadComplete_ = true;
}
