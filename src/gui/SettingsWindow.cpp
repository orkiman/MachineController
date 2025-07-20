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
#include <vector>

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

    // Connections are now handled by Qt's auto-connect feature via on_<objectName>_<signalName>() naming
    // and direct connections in the constructor

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

    // Connect signals for communication tab
    // NOTE: These UI elements don't exist in the UI file, so connections are commented out
    // connect(ui->addCommunicationButton, &QPushButton::clicked, this, &SettingsWindow::on_addCommunicationButton_clicked);
    // connect(ui->removeCommunicationButton, &QPushButton::clicked, this, &SettingsWindow::on_removeCommunicationButton_clicked);
    // connect(ui->communicationTable, &QTableWidget::cellChanged, this, &SettingsWindow::onCommunicationCellChanged);
    
    // Connect signals for glue tab
    connect(ui->glueRowsTable, &QTableWidget::cellChanged, this, &SettingsWindow::onGlueRowCellChanged);
    // Connect glue controller selector to our specific handler (not using auto-connect naming)
    connect(ui->glueControllerSelectorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::onGlueControllerSelectorChanged);
    connect(ui->gluePlanSelectorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::onGluePlanSelectorChanged);
    
    // Note: All glue tab elements use Qt's auto-connect feature via on_<objectName>_<signalName>() naming convention
    // This includes: buttons, line edits, and other combo boxes - no manual connections needed

    // Fill all tab fields with current config values
    fillCommunicationTabFields();
    fillTimersTabFields();
    fillIOTabFields();
    fillDataFileTabFields();
    populateGlueCommunicationComboBox();
    fillGlueTabFields();


    // Timers table: auto-save on duration edit
    connect(ui->timersTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        // Only handle duration column (column 1) and only if not refreshing
        if (item && item->column() == 1 && !isRefreshing_) {
            saveTimersToConfig();
            item->setForeground(QBrush(Qt::black));
        }
    });
    isRefreshing_ = false;

    // Ensure communication type visibility is correct after setup
    QMetaObject::invokeMethod(this, [this]() { updateCommunicationTypeVisibility(-1); }, Qt::QueuedConnection);
    
    // Connect communicationActiveCheckBox on all communication pages to prevent Qt auto-connect issues
    for (int i = 0; i < commStack->count(); ++i) {
        QWidget* commPage = commStack->widget(i);
        if (commPage) {
            QComboBox* typeComboBox = commPage->findChild<QComboBox*>("communicationTypeComboBox");
            if (typeComboBox) {
                connect(typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &SettingsWindow::updateCommunicationTypeVisibility);
            }
            // Manually connect communicationActiveCheckBox to prevent double connections
            // (Qt auto-connect would connect to all checkboxes with same name across all pages)
            QCheckBox* activeCheckBox = commPage->findChild<QCheckBox*>("communicationActiveCheckBox");
            if (activeCheckBox) {
                connect(activeCheckBox, &QCheckBox::stateChanged, this, &SettingsWindow::onCommunicationActiveCheckBoxChanged);
            }
            // SendPushButton handled by Qt auto-connect
        }
    }

    // Send initial controller setup to ensure hardware is in sync with UI
    QMetaObject::invokeMethod(this, &SettingsWindow::sendControllerSetupToActiveController, Qt::QueuedConnection);
    
    isRefreshing_ = false;
}
// ----------------------------------------------------------------------------
// SettingsWindow Destructor
// ----------------------------------------------------------------------------
SettingsWindow::~SettingsWindow() {
    delete ui;
}

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
        saveCurrentCommunicationSettings();
        
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
    saveTimersToConfig();
    
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

// Populate the Glue tab with controllers and plans from config
void SettingsWindow::fillGlueTabFields()
{
    isRefreshing_ = true;
    
    // Block signals during update
    ui->glueControllerSelectorComboBox->blockSignals(true);
    ui->gluePlanSelectorComboBox->blockSignals(true);
    ui->glueRowsTable->blockSignals(true);
    
    // Clear existing items
    ui->glueControllerSelectorComboBox->clear();
    ui->gluePlanSelectorComboBox->clear();
    ui->glueRowsTable->setRowCount(0);
    
    if (!config_) {
        getLogger()->warn("[fillGlueTabFields] Config object is null. Cannot load glue settings.");
        isRefreshing_ = false;
        return;
    }
    
    try {
        // Get glue settings from config
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Get the active controller ID from settings
        std::string activeControllerId;
        if (glueSettings.contains("activeController") && glueSettings["activeController"].is_string()) {
            activeControllerId = glueSettings["activeController"].get<std::string>();
        }
        
        // Check if controllers exist
        if (glueSettings.contains("controllers") && glueSettings["controllers"].is_object()) {
            // Add controllers to combo box
            for (auto& [controllerId, controller] : glueSettings["controllers"].items()) {
                if (controller.is_object()) {
                    // Get the controller name
                    QString name;
                    if (controller.contains("name") && controller["name"].is_string()) {
                        name = QString::fromStdString(controller["name"].get<std::string>());
                    }
                    
                    // Create display text as "[Controller ID]: [Name]" or just ID if name is empty
                    QString displayText = QString::fromStdString(controllerId);
                    if (!name.isEmpty()) {
                        displayText += ": " + name;
                    }
                    
                    ui->glueControllerSelectorComboBox->addItem(displayText, QString::fromStdString(controllerId));
                }
            }
            
            // Populate communication combo box
            populateGlueCommunicationComboBox();
            
            // Restore the active controller selection
            int activeIndex = -1;
            if (!activeControllerId.empty()) {
                activeIndex = ui->glueControllerSelectorComboBox->findData(QString::fromStdString(activeControllerId));
            }
            
            if (activeIndex != -1) {
                ui->glueControllerSelectorComboBox->setCurrentIndex(activeIndex);
            } else if (ui->glueControllerSelectorComboBox->count() > 0) {
                ui->glueControllerSelectorComboBox->setCurrentIndex(0);
            }
            
            // Load the selected controller's data
            if (ui->glueControllerSelectorComboBox->count() > 0) {
                currentGlueControllerName_ = ui->glueControllerSelectorComboBox->currentData().toString().toStdString();
                onGlueControllerSelectorChanged(ui->glueControllerSelectorComboBox->currentIndex());
            } else {
                ui->glueStackedWidget->setEnabled(false);
            }
        }
    } catch (const std::exception& e) {
        getLogger()->warn("[fillGlueTabFields] Exception while loading glue settings: {}", e.what());
    }
    
    // Reconnect signals
    ui->glueControllerSelectorComboBox->blockSignals(false);
    ui->gluePlanSelectorComboBox->blockSignals(false);
    ui->glueRowsTable->blockSignals(false);
    
    isRefreshing_ = false;
}

// Populate the glue communication combo box with communication names
void SettingsWindow::populateGlueCommunicationComboBox()
{
    // Store the current selection
    QString currentSelection = ui->glueCommunicationComboBox->currentText();
    
    // Clear the combo box
    ui->glueCommunicationComboBox->clear();
    
    // Get communication settings from config to populate with communication names
    if (config_) {
        try {
            nlohmann::json commSettings = config_->getCommunicationSettings();
            
            getLogger()->debug("[populateGlueCommunicationComboBox] commSettings length: {}", commSettings.size());
            getLogger()->debug("[populateGlueCommunicationComboBox] Full JSON: {}", commSettings.dump(2));
            
            // Add communication names (communication1, communication2, etc.)
            int loopCount = 0;
            for (const auto& [commName, commData] : commSettings.items()) {
                loopCount++;
                getLogger()->debug("[populateGlueCommunicationComboBox] Loop {}: Processing '{}'", loopCount, commName);
                
                if (commData.is_object() && commData.contains("active")) {
                    bool isActive = commData["active"].get<bool>();
                    getLogger()->debug("[populateGlueCommunicationComboBox] '{}' active: {}", commName, isActive);
                    
                    if (isActive) {
                        // Only add active communication channels if not already present
                        QString commNameQt = QString::fromStdString(commName);
                        if (ui->glueCommunicationComboBox->findText(commNameQt) == -1) {
                            ui->glueCommunicationComboBox->addItem(commNameQt);
                            getLogger()->debug("[populateGlueCommunicationComboBox] Added '{}' to combo box", commName);
                        } else {
                            getLogger()->warn("[populateGlueCommunicationComboBox] '{}' already exists in combo box, skipping", commName);
                        }
                    }
                } else {
                    getLogger()->warn("[populateGlueCommunicationComboBox] '{}' is not an object or missing 'active' field", commName);
                }
            }
            
            getLogger()->debug("[populateGlueCommunicationComboBox] Total loop iterations: {}", loopCount);
        } catch (const std::exception& e) {
            getLogger()->warn("[populateGlueCommunicationComboBox] Exception: {}", e.what());
        }
    }
    
    // Restore the previous selection if it exists
    int index = ui->glueCommunicationComboBox->findText(currentSelection);
    if (index >= 0) {
        ui->glueCommunicationComboBox->setCurrentIndex(index);
    }
}

// Handle glue controller selection change
void SettingsWindow::onGlueControllerSelectorChanged(int index)
{
    if (!config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // If not refreshing, save the new active controller and reload
    if (!isRefreshing_) {
        saveActiveGlueController();
        fillGlueTabFields();
        return;
    }
    
    // Save current controller settings before switching (but don't reload yet)
    if (!currentGlueControllerName_.empty()) {
        saveCurrentControllerWithoutReload();
    }
    
    // Get the selected controller ID
    if (index >= 0 && index < ui->glueControllerSelectorComboBox->count()) {
        currentGlueControllerName_ = ui->glueControllerSelectorComboBox->itemData(index).toString().toStdString();
    } else {
        currentGlueControllerName_ = "";
        return;
    }
    
    // Block signals during update
    isRefreshing_ = true;
    ui->gluePlanSelectorComboBox->blockSignals(true);
    ui->glueRowsTable->blockSignals(true);
    
    try {
        // Get glue settings from config
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if the selected controller exists
        if (glueSettings.contains("controllers") && 
            glueSettings["controllers"].contains(currentGlueControllerName_)) {
            
            const auto& controller = glueSettings["controllers"][currentGlueControllerName_];
            
            // Enable the controller UI
            ui->glueStackedWidget->setEnabled(true);
            
            // Set controller name
            std::string controllerName = "";
            if (controller.contains("name") && controller["name"].is_string()) {
                controllerName = controller["name"].get<std::string>();
            } else if (controller.contains("description") && controller["description"].is_string()) {
                // For backward compatibility with configs that use description
                controllerName = controller["description"].get<std::string>();
            }
            ui->glueControllerNameLineEdit->setText(QString::fromStdString(controllerName));
            
            // Set communication port
            std::string commPortStr = "";
            if (controller.contains("communication") && controller["communication"].is_string()) {
                commPortStr = controller["communication"].get<std::string>();
            }
            QString commPort = QString::fromStdString(commPortStr);
            int commIndex = ui->glueCommunicationComboBox->findText(commPort);
            if (commIndex >= 0) {
                ui->glueCommunicationComboBox->setCurrentIndex(commIndex);
            }
            
            // Set type (dots/line)
            std::string typeStr = "dots"; // Default value
            if (controller.contains("type") && controller["type"].is_string()) {
                typeStr = controller["type"].get<std::string>();
            }
            QString type = QString::fromStdString(typeStr);
            int typeIndex = ui->glueTypeComboBox->findText(type, Qt::MatchFixedString);
            if (typeIndex >= 0) {
                ui->glueTypeComboBox->setCurrentIndex(typeIndex);
            }
            
            // Set encoder value
            double encoder = 1.0; // Default value
            if (controller.contains("encoder")) {
                if (controller["encoder"].is_number()) {
                    encoder = controller["encoder"].get<double>();
                }
            }
            ui->glueEncoderSpinBox->setValue(encoder);
            
            // Set page length value
            int pageLength = 100; // Default value
            if (controller.contains("pageLength")) {
                if (controller["pageLength"].is_number()) {
                    pageLength = controller["pageLength"].get<int>();
                }
            }
            ui->gluePageLengthSpinBox->setValue(pageLength);
            
            // Set enabled state
            bool enabled = true; // Default value
            if (controller.contains("enabled")) {
                if (controller["enabled"].is_boolean()) {
                    enabled = controller["enabled"].get<bool>();
                }
            }
            ui->glueControllerEnabledCheckBox->setChecked(enabled);
            
            // Set start current (A)
            double startCurrent = 1.0; // Default value (1.0A)
            if (controller.contains("startCurrent") && controller["startCurrent"].is_number()) {
                startCurrent = controller["startCurrent"].get<double>();
            }
            ui->glueStartCurrentSpinBox->setValue(startCurrent);
            
            // Set start duration (ms)
            double startDurationMS = 0.5; // Default value (0.5ms)
            if (controller.contains("startDurationMS") && controller["startDurationMS"].is_number()) {
                startDurationMS = controller["startDurationMS"].get<double>();
            }
            ui->glueStartDurationSpinBox->setValue(startDurationMS);
            
            // Set hold current (A)
            double holdCurrent = 0.5; // Default value (0.5A)
            if (controller.contains("holdCurrent") && controller["holdCurrent"].is_number()) {
                holdCurrent = controller["holdCurrent"].get<double>();
            }
            ui->glueHoldCurrentSpinBox->setValue(holdCurrent);
            
            // Set dot size (small/medium/large)
            std::string dotSize = "medium"; // Default value
            if (controller.contains("dotSize") && controller["dotSize"].is_string()) {
                dotSize = controller["dotSize"].get<std::string>();
            }
            QString qDotSize = QString::fromStdString(dotSize);
            int dotSizeIndex = ui->glueDotSizeComboBox->findText(qDotSize, Qt::MatchFixedString);
            if (dotSizeIndex >= 0) {
                ui->glueDotSizeComboBox->setCurrentIndex(dotSizeIndex);
            }
            
            // Clear and populate plan selector
            ui->gluePlanSelectorComboBox->clear();
            
            // Check if plans exist for this controller
            if (controller.contains("plans") && controller["plans"].is_object()) {
                // Add plans to combo box
                for (auto& [planId, plan] : controller["plans"].items()) {
                    if (plan.is_object() && plan.contains("name")) {
                        QString name = QString::fromStdString(plan["name"].get<std::string>());
                        
                        // Create display text as "[Plan ID]: [Name]" or just ID if name is empty
                        QString displayText = QString::fromStdString(planId);
                        if (!name.isEmpty()) {
                            displayText += ": " + name;
                        }
                        
                        ui->gluePlanSelectorComboBox->addItem(displayText, QString::fromStdString(planId));
                    }
                }
            }
            
            // If there are plans, restore the active plan or select the first one
            if (ui->gluePlanSelectorComboBox->count() > 0) {
                // Try to restore the active plan for this controller
                int activePlanIndex = 0; // Default to first plan
                
                if (controller.contains("activePlan") && controller["activePlan"].is_string()) {
                    std::string activePlanId = controller["activePlan"].get<std::string>();
                    int foundIndex = ui->gluePlanSelectorComboBox->findData(QString::fromStdString(activePlanId));
                    if (foundIndex >= 0) {
                        activePlanIndex = foundIndex;
                    }
                }
                
                ui->gluePlanSelectorComboBox->setCurrentIndex(activePlanIndex);
                currentGluePlanName_ = ui->gluePlanSelectorComboBox->itemData(activePlanIndex).toString().toStdString();
                
                // Populate plan fields
                onGluePlanSelectorChanged(activePlanIndex);
            } else {
                // No plans, clear the plan UI
                currentGluePlanName_ = "";
                ui->gluePlanNameLineEdit->clear();
                ui->glueRowsTable->setRowCount(0);
            }
            
            // Update visibility of space column based on type
            updateGlueTypeVisibility(ui->glueTypeComboBox->currentIndex());
        } else {
            // Controller not found, disable UI
            ui->glueStackedWidget->setEnabled(false);
            currentGlueControllerName_ = "";
        }
    } catch (const std::exception& e) {
        getLogger()->warn("[onGlueControllerSelectorChanged] Exception: {}", e.what());
    }
    
    // Unblock signals
    ui->gluePlanSelectorComboBox->blockSignals(false);
    ui->glueRowsTable->blockSignals(false);
    isRefreshing_ = false;
    
    // Send the controller setup to the Arduino
    if (!currentGlueControllerName_.empty()) {
        QMetaObject::invokeMethod(this, [this]() {
            sendControllerSetupToActiveController();
        }, Qt::QueuedConnection);
    }
}

// Handle glue plan selection change
void SettingsWindow::onGluePlanSelectorChanged(int index)
{
    if (!config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // If not refreshing, save the active plan, reload, and update controller
    if (!isRefreshing_) {
        // Get the selected plan ID
        if (index >= 0 && index < ui->gluePlanSelectorComboBox->count()) {
            std::string newPlanId = ui->gluePlanSelectorComboBox->itemData(index).toString().toStdString();
            
            // Only proceed if the plan actually changed
            if (newPlanId != currentGluePlanName_) {
                currentGluePlanName_ = newPlanId;
                saveActivePlanForController();
                fillGlueTabFields();
                
                // Send the new plan to the controller
                sendControllerSetupToActiveController();
            }
        }
        return;
    }
    
    // When refreshing, just load the selected plan data - DO NOT save old plan data
    
    // Get the selected plan ID
    if (index >= 0 && index < ui->gluePlanSelectorComboBox->count()) {
        currentGluePlanName_ = ui->gluePlanSelectorComboBox->itemData(index).toString().toStdString();
    } else {
        currentGluePlanName_ = "";
        return;
    }
    
    // Block signals during update
    isRefreshing_ = true;
    ui->glueRowsTable->blockSignals(true);
    
    try {
        // Get glue settings from config
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if the selected controller and plan exist
        if (glueSettings.contains("controllers") && 
            glueSettings["controllers"].contains(currentGlueControllerName_) &&
            glueSettings["controllers"][currentGlueControllerName_].contains("plans") &&
            glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentGluePlanName_)) {
            
            const auto& plan = glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_];
            
            // Set plan name
            std::string planName = "";
            if (plan.contains("name") && plan["name"].is_string()) {
                planName = plan["name"].get<std::string>();
            }
            ui->gluePlanNameLineEdit->setText(QString::fromStdString(planName));
            
            // Clear the rows table
            ui->glueRowsTable->setRowCount(0);
            
            // Initialize the guns array if it doesn't exist
            if (!plan.contains("guns") || !plan["guns"].is_array()) {
                // Create default guns array if it doesn't exist
                nlohmann::json defaultGuns = nlohmann::json::array();
                for (int i = 0; i < 4; ++i) {
                    nlohmann::json gun;
                    gun["gunId"] = i + 1;
                    gun["enabled"] = (i == 0); // Enable first gun by default
                    gun["rows"] = nlohmann::json::array();
                    defaultGuns.push_back(gun);
                }
                
                // Update the plan with default guns
                const_cast<nlohmann::json&>(plan) = plan;
                const_cast<nlohmann::json&>(plan)["guns"] = defaultGuns;
                
                // Save the updated settings
                Config* mutableConfig = const_cast<Config*>(config_);
                nlohmann::json glueSettings = config_->getGlueSettings();
                glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_] = plan;
                mutableConfig->updateGlueSettings(glueSettings);
                mutableConfig->saveToFile();
            }
            
            // Load the first gun's data by default
            loadCurrentGunData(0);
            
            // Update the gun selector
            ui->gunSelectorComboBox->blockSignals(true);
            ui->gunSelectorComboBox->setCurrentIndex(0);
            ui->gunSelectorComboBox->blockSignals(false);
            
            // Load sensor offset
            int sensorOffset = plan.value("sensorOffset", 10); // Default to 10 if not found
            ui->gluePlanSensorOffsetSpinBox->setValue(sensorOffset);
            
        } else {
            // Plan not found, clear the UI
            ui->gluePlanNameLineEdit->clear();
            ui->glueRowsTable->setRowCount(0);
            ui->gluePlanSensorOffsetSpinBox->setValue(10); // Reset to default
            currentGluePlanName_ = "";
        }
    } catch (const std::exception& e) {
        getLogger()->warn("[onGluePlanSelectorChanged] Exception: {}", e.what());
    }
    
    // Unblock signals
    ui->glueRowsTable->blockSignals(false);
    isRefreshing_ = false;
}

// Update visibility of space column based on glue type
void SettingsWindow::updateGlueTypeVisibility(int index)
{
    // Get the current type
    QString type = ui->glueTypeComboBox->itemText(index);
    
    // Show/hide space column based on type
    bool showSpace = (type.toLower() == "dots");
    
    // Get the space column index
    int spaceColumnIndex = 2; // Assuming columns are: from(0), to(1), space(2)
    
    // Show/hide the space column
    ui->glueRowsTable->setColumnHidden(spaceColumnIndex, !showSpace);
    
    // Update table header
    QStringList headers;
    headers << "From" << "To";
    if (showSpace) {
        headers << "Space";
    }
    ui->glueRowsTable->setHorizontalHeaderLabels(headers);
}

// Save current controller settings without triggering reload (used when switching controllers)
void SettingsWindow::saveCurrentControllerWithoutReload()
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        if (!glueSettings.contains("controllers")) {
            glueSettings["controllers"] = nlohmann::json::object();
        }
        
        nlohmann::json& controller = glueSettings["controllers"][currentGlueControllerName_];
        controller["name"] = ui->glueControllerNameLineEdit->text().toStdString();
        controller["communication"] = ui->glueCommunicationComboBox->currentText().toStdString();
        controller["type"] = ui->glueTypeComboBox->currentText().toLower().toStdString();
        controller["encoder"] = ui->glueEncoderSpinBox->value();
        
        if (!controller.contains("plans")) {
            controller["plans"] = nlohmann::json::object();
        }
        
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveCurrentControllerWithoutReload] Failed to save settings to file");
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[saveCurrentControllerWithoutReload] Exception: {}", e.what());
    }
}

// Individual field save functions that immediately save and reload
void SettingsWindow::onGlueControllerNameChanged()
{
    if (!isRefreshing_) {
        saveCurrentGlueControllerSettings();
    }
}

// Save the active glue controller ID to config
void SettingsWindow::saveActiveGlueController()
{
    if (!config_ || ui->glueControllerSelectorComboBox->currentIndex() < 0) {
        return;
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Save the active controller ID
        QString activeControllerId = ui->glueControllerSelectorComboBox->currentData().toString();
        glueSettings["activeController"] = activeControllerId.toStdString();
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveActiveGlueController] Failed to save settings to file");
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[saveActiveGlueController] Exception: {}", e.what());
    }
}

// Save the active plan ID for the current controller
void SettingsWindow::saveActivePlanForController()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if controller exists
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_)) {
            return;
        }
        
        // Save the active plan ID for this controller
        glueSettings["controllers"][currentGlueControllerName_]["activePlan"] = currentGluePlanName_;
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveActivePlanForController] Failed to save settings to file");
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[saveActivePlanForController] Exception: {}", e.what());
    }
}

// Save current glue controller settings and reload tab
void SettingsWindow::saveCurrentGlueControllerSettings()
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Ensure controllers object exists
        if (!glueSettings.contains("controllers")) {
            glueSettings["controllers"] = nlohmann::json::object();
        }
        
        // Update controller properties
        nlohmann::json& controller = glueSettings["controllers"][currentGlueControllerName_];
        controller["name"] = ui->glueControllerNameLineEdit->text().toStdString();
        controller["communication"] = ui->glueCommunicationComboBox->currentText().toStdString();
        controller["type"] = ui->glueTypeComboBox->currentText().toLower().toStdString();
        controller["encoder"] = ui->glueEncoderSpinBox->value();
        controller["pageLength"] = ui->gluePageLengthSpinBox->value();
        controller["enabled"] = ui->glueControllerEnabledCheckBox->isChecked();
        
        // Ensure plans object exists
        if (!controller.contains("plans")) {
            controller["plans"] = nlohmann::json::object();
        }
        
        // Save active controller and update settings
        glueSettings["activeController"] = currentGlueControllerName_;
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveCurrentGlueControllerSettings] Failed to save settings to file");
        }
        
        // Reload the tab to reflect changes
        fillGlueTabFields();
        
        // Send updated controller setup to Arduino
        sendControllerSetupToActiveController();
        
    } catch (const std::exception& e) {
        getLogger()->warn("[saveCurrentGlueControllerSettings] Exception: {}", e.what());
    }
}

// saveCurrentGluePlanSettings has been removed - functionality moved to individual handlers

// Save current glue plan settings without reloading tab
void SettingsWindow::saveCurrentPlanWithoutReload()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_) ||
            !glueSettings["controllers"][currentGlueControllerName_].contains("plans")) {
            return;
        }
        
        nlohmann::json& plan = glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_];
        plan["name"] = ui->gluePlanNameLineEdit->text().toStdString();
        
        // Get the current gun index from the selector
        int gunIndex = ui->gunSelectorComboBox->currentIndex();
        if (gunIndex < 0) {
            return; // No gun selected
        }
        
        // Get or create guns array
        if (!plan.contains("guns") || !plan["guns"].is_array()) {
            plan["guns"] = nlohmann::json::array();
        }
        
        // Ensure we have enough guns in the array
        while (plan["guns"].size() <= static_cast<size_t>(gunIndex)) {
            plan["guns"].push_back(nlohmann::json::object());
        }
        
        // Get the current gun
        nlohmann::json& gun = plan["guns"][gunIndex];
        
        // Save gun enabled state
        gun["gunId"] = gunIndex + 1; // 1-based index for gun ID
        gun["enabled"] = ui->gunEnabledCheckBox->isChecked();
        
        // Save rows from table
        nlohmann::json rows = nlohmann::json::array();
        for (int i = 0; i < ui->glueRowsTable->rowCount(); ++i) {
            nlohmann::json row;
            
            // Get 'from' value
            QTableWidgetItem* fromItem = ui->glueRowsTable->item(i, 0);
            if (fromItem) {
                row["from"] = fromItem->text().toInt();
            } else {
                row["from"] = 0;
            }
            
            // Get 'to' value
            QTableWidgetItem* toItem = ui->glueRowsTable->item(i, 1);
            if (toItem) {
                row["to"] = toItem->text().toInt();
            } else {
                row["to"] = 0;
            }
            
            // Get 'space' value
            QTableWidgetItem* spaceItem = ui->glueRowsTable->item(i, 2);
            if (spaceItem) {
                row["space"] = spaceItem->text().toDouble();
            } else {
                row["space"] = 0.0;
            }
            
            rows.push_back(row);
        }
        
        // Save rows under the current gun
        gun["rows"] = rows;
        
        // Update the plan in the settings
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveCurrentPlanWithoutReload] Failed to save settings to file");
        }
        
        // Note: No fillGlueTabFields() call here - that's the key difference
        
    } catch (const std::exception& e) {
        getLogger()->warn("[saveCurrentPlanWithoutReload] Exception: {}", e.what());
    }
}

// Add a row to the glue plan table
void SettingsWindow::addGlueRowToTable(int from, int to, double space)
{
    // Block signals to prevent multiple change events
    bool wasBlocked = ui->glueRowsTable->blockSignals(true);
    
    try {
        // Get current row count
        int row = ui->glueRowsTable->rowCount();
        
        // Insert a new row
        ui->glueRowsTable->insertRow(row);
        
        // Create and set table items
        QTableWidgetItem* fromItem = new QTableWidgetItem(QString::number(from));
        QTableWidgetItem* toItem = new QTableWidgetItem(QString::number(to));
        QTableWidgetItem* spaceItem = new QTableWidgetItem(QString::number(space));
        
        // Add items to the table
        ui->glueRowsTable->setItem(row, 0, fromItem);
        ui->glueRowsTable->setItem(row, 1, toItem);
        ui->glueRowsTable->setItem(row, 2, spaceItem);
        
        // Hide space column if type is line
        if (ui->glueTypeComboBox->currentText().toLower() != "dots") {
            ui->glueRowsTable->setColumnHidden(2, true);
        }
    } catch (...) {
        // Make sure to restore signal blocking state even if an exception occurs
        ui->glueRowsTable->blockSignals(wasBlocked);
        throw;
    }
    
    // Restore signal blocking state
    ui->glueRowsTable->blockSignals(wasBlocked);
}

// Handle adding a new glue controller
void SettingsWindow::on_addGlueControllerButton_clicked()
{
    if (!config_) {
        return;
    }
    
    // Save current controller settings
    if (!currentGlueControllerName_.empty()) {
        saveCurrentGlueControllerSettings();
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if controllers object exists
        if (!glueSettings.contains("controllers")) {
            glueSettings["controllers"] = nlohmann::json::object();
        }
        
        // Generate a unique ID for the new controller starting from 1
        std::string newControllerId;
        int id = 1;
        do {
            newControllerId = "controller_" + std::to_string(id);
            id++;
        } while (glueSettings["controllers"].contains(newControllerId));
        
        // Create new controller with default values
        nlohmann::json newController = {
            {"name", "New Controller"},
            {"communication", ui->glueCommunicationComboBox->currentText().toStdString()},
            {"type", "dots"},
            {"encoder", 1.0},
            {"enabled", true},
            {"pageLength", 100},
            {"startCurrent", 1.0},  // Default start current: 1.0A
            {"startDurationMS", 0.5}, // Default start duration: 0.5ms
            {"holdCurrent", 0.5},   // Default hold current: 0.5A
            {"dotSize", "medium"},  // Default dot size: medium
            {"plans", nlohmann::json::object()}
        };
        
        // Add a default plan
        std::string newPlanId = "plan_1";
        nlohmann::json defaultPlan = {
            {"name", newPlanId},
            {"sensorOffset", 10},
            {"guns", nlohmann::json::array()}
        };

        // Add 4 default guns
        for (int i = 1; i <= 4; ++i) {
            nlohmann::json gun = {
                {"enabled", false},
                {"gunId", i},
                {"rows", nlohmann::json::array()}
            };
            defaultPlan["guns"].push_back(gun);
        }

        // Add the plan to the controller and set it as active
        newController["plans"][newPlanId] = defaultPlan;
        newController["activePlan"] = newPlanId;
        
        // Set as active controller
        glueSettings["activeController"] = newControllerId;
        
        // Add new controller to settings
        glueSettings["controllers"][newControllerId] = newController;
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_addGlueControllerButton_clicked] Failed to save settings to file");
        }
        
        // Refresh the glue tab
        fillGlueTabFields();
        
        // Select the new controller
        for (int i = 0; i < ui->glueControllerSelectorComboBox->count(); ++i) {
            if (ui->glueControllerSelectorComboBox->itemData(i).toString() == QString::fromStdString(newControllerId)) {
                ui->glueControllerSelectorComboBox->setCurrentIndex(i);
                break;
            }
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_addGlueControllerButton_clicked] Exception: {}", e.what());
    }
}

// Handle removing a glue controller
void SettingsWindow::on_removeGlueControllerButton_clicked()
{
    if (!config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if controllers object exists and contains the current controller
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_)) {
            return;
        }
        
        // Remove the controller
        glueSettings["controllers"].erase(currentGlueControllerName_);
        
        // Find the next available controller to set as active
        std::string nextActiveController = "";
        if (glueSettings.contains("controllers") && !glueSettings["controllers"].empty()) {
            for (auto it = glueSettings["controllers"].begin(); 
                 it != glueSettings["controllers"].end(); ++it) {
                if (it.key() != currentGlueControllerName_) {
                    nextActiveController = it.key();
                    break;
                }
            }
        }
        
        // Update active controller in settings
        glueSettings["activeController"] = nextActiveController;
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Save to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_removeGlueControllerButton_clicked] Failed to save settings to file");
        }
        
        // Update current controller name
        currentGlueControllerName_ = nextActiveController;
        currentGluePlanName_ = "";
        
        // If we have a new active controller, update the UI
        if (!nextActiveController.empty()) {
            // Force UI update by setting index to -1 and then back to the new controller
            ui->glueControllerSelectorComboBox->setCurrentIndex(-1);
            int index = ui->glueControllerSelectorComboBox->findText(QString::fromStdString(nextActiveController));
            if (index >= 0) {
                ui->glueControllerSelectorComboBox->setCurrentIndex(index);
            }
        }
        
        // Refresh the glue tab
        fillGlueTabFields();
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_removeGlueControllerButton_clicked] Exception: {}", e.what());
    }
}

// Handle adding a new glue plan
void SettingsWindow::on_addGluePlanButton_clicked()
{
    if (!config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // Don't save current gun settings here as it causes the old plan to be sent
    // We'll handle saving the current gun settings after adding the new plan
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if controller exists
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_)) {
            return;
        }
        
        // Get controller object
        nlohmann::json& controller = glueSettings["controllers"][currentGlueControllerName_];
        
        // Ensure plans object exists
        if (!controller.contains("plans")) {
            controller["plans"] = nlohmann::json::object();
        }
        
        // Generate a unique ID for the new plan
        std::string newPlanId = "plan_" + std::to_string(controller["plans"].size() + 1);
        while (controller["plans"].contains(newPlanId)) {
            newPlanId = "plan_" + std::to_string(std::rand());
        }
        
        // Create new plan with default values and guns array
        nlohmann::json newPlan = {
            {"name", "New Plan"},
            {"sensorOffset", 10},  // Default sensor offset
            {"guns", nlohmann::json::array()}
        };
        
        // Add 4 default guns with empty rows
        for (int i = 0; i < 4; ++i) {
            nlohmann::json gun = {
                {"gunId", i + 1},
                {"enabled", (i == 0)},  // Only enable first gun by default
                {"rows", nlohmann::json::array()}
            };
            
            // Add default row to first gun
            if (i == 0) {
                gun["rows"].push_back({
                    {"from", 0},
                    {"to", 100},
                    {"space", 10.0}
                });
            }
            
            newPlan["guns"].push_back(gun);
        }
        
        // Add the new plan to the controller
        controller["plans"][newPlanId] = newPlan;
        
        // Set the new plan as active
        controller["activePlan"] = newPlanId;
        currentGluePlanName_ = newPlanId;
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Save the active plan to config
        saveActivePlanForController();
        
        // Refresh the glue tab
        fillGlueTabFields();
        
        // Select the new plan in the UI
        for (int i = 0; i < ui->gluePlanSelectorComboBox->count(); ++i) {
            if (ui->gluePlanSelectorComboBox->itemData(i).toString() == QString::fromStdString(newPlanId)) {
                ui->gluePlanSelectorComboBox->setCurrentIndex(i);
                break;
            }
        }
        
        // Send updated controller setup to Arduino
        sendControllerSetupToActiveController();
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_addGluePlanButton_clicked] Exception: {}", e.what());
    }
}

// Handle removing a glue plan
void SettingsWindow::on_removeGluePlanButton_clicked()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    try {
        // Get current glue settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        
        // Check if controller and plan exist
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_) ||
            !glueSettings["controllers"][currentGlueControllerName_].contains("plans") ||
            !glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentGluePlanName_)) {
            return;
        }
        
                // Check if the plan being removed is the active plan
        bool wasActivePlan = false;
        if (glueSettings["controllers"][currentGlueControllerName_].contains("activePlan") &&
            glueSettings["controllers"][currentGlueControllerName_]["activePlan"] == currentGluePlanName_) {
            wasActivePlan = true;
        }
        
        // Remove the plan
        glueSettings["controllers"][currentGlueControllerName_]["plans"].erase(currentGluePlanName_);
        
        // If the removed plan was active, update the active plan
        if (wasActivePlan) {
            // Find another plan to make active (first one available)
            std::string newActivePlan = "";
            if (!glueSettings["controllers"][currentGlueControllerName_]["plans"].empty()) {
                newActivePlan = glueSettings["controllers"][currentGlueControllerName_]["plans"].begin().key();
            }
            
            // Update the active plan in settings
            glueSettings["controllers"][currentGlueControllerName_]["activePlan"] = newActivePlan;
            
            // Update currentGluePlanName_ if we found a new active plan
            if (!newActivePlan.empty()) {
                currentGluePlanName_ = newActivePlan;
            } else {
                currentGluePlanName_ = "";
            }
        }
        
        // Update glue settings in config
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_removeGluePlanButton_clicked] Failed to save settings to file");
        }
        
        // Clear current plan name if we didn't set a new active plan
        if (!wasActivePlan || currentGluePlanName_.empty()) {
            currentGluePlanName_ = "";
        }
        
        // Refresh the glue tab (this will update the UI including the plan selector)
        fillGlueTabFields();
        
        // Save the active plan to config
        saveActivePlanForController();
        
        // Send updated controller setup to Arduino
        sendControllerSetupToActiveController();
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_removeGluePlanButton_clicked] Exception: {}", e.what());
    }
}

// Handle adding a new row to the glue plan
void SettingsWindow::on_addGlueRowButton_clicked()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    // Block signals to prevent multiple change events
    bool wasBlocked = ui->glueRowsTable->blockSignals(true);
    
    try {
        // Add a new row with default values
        addGlueRowToTable(0, 100, 10.0);
        
        // Save changes for the current gun
        saveCurrentGunSettings();
    } catch (...) {
        // Make sure to restore signal blocking state even if an exception occurs
        ui->glueRowsTable->blockSignals(wasBlocked);
        throw;
    }
    
    // Restore signal blocking state
    ui->glueRowsTable->blockSignals(wasBlocked);
}

// Handle removing a row from the glue plan
void SettingsWindow::on_removeGlueRowButton_clicked()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    // Get selected row
    int selectedRow = ui->glueRowsTable->currentRow();
    
    // Remove the row if valid
    if (selectedRow >= 0 && selectedRow < ui->glueRowsTable->rowCount()) {
        ui->glueRowsTable->removeRow(selectedRow);
        
        // Save changes for the current gun
        saveCurrentGunSettings();
    }
}

// Handle controller name text change
void SettingsWindow::on_glueControllerNameLineEdit_textChanged(const QString& text)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // Update the controller name in the combo box
    int currentIndex = ui->glueControllerSelectorComboBox->currentIndex();
    if (currentIndex >= 0) {
        // Format as "[Controller ID]: [Name]" to preserve the ID part
        QString controllerId = QString::fromStdString(currentGlueControllerName_);
        QString displayText = controllerId;
        if (!text.isEmpty()) {
            displayText += ": " + text;
        }
        ui->glueControllerSelectorComboBox->setItemText(currentIndex, displayText);
    }
    
    // Save changes
    saveCurrentGlueControllerSettings();
    
}

// Handle communication combo box change
void SettingsWindow::on_glueCommunicationComboBox_currentIndexChanged(int index)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // Save changes
    saveCurrentGlueControllerSettings();
    
}

// Handle glue type combo box change
void SettingsWindow::on_glueTypeComboBox_currentIndexChanged(int index)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // Update visibility of space column
    updateGlueTypeVisibility(index);
    
    // Save changes
    saveCurrentGlueControllerSettings();
    
}

// Handle encoder value change
void SettingsWindow::on_glueEncoderSpinBox_valueChanged(double value)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    // Save changes
    saveCurrentGlueControllerSettings();
    
}

// Handle page length change
void SettingsWindow::on_gluePageLengthSpinBox_valueChanged(int value)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_)) {
            return;
        }
        
        // Save page length directly
        glueSettings["controllers"][currentGlueControllerName_]["pageLength"] = value;
        
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_gluePageLengthSpinBox_valueChanged] Failed to save settings to file");
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_gluePageLengthSpinBox_valueChanged] Exception: {}", e.what());
    }
}

// Handle calibrate button click
void SettingsWindow::on_glueCalibrateButton_clicked()
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        int pageLength = ui->gluePageLengthSpinBox->value();
        std::string controllerName = currentGlueControllerName_;
        
        getLogger()->info("[on_glueCalibrateButton_clicked] Starting encoder calibration for controller '{}' with page length {} mm", 
                         controllerName, pageLength);
        
        // Get the communication port for this controller
        std::string communicationPort = ui->glueCommunicationComboBox->currentText().toStdString();
        if (communicationPort.empty()) {
            getLogger()->warn("[on_glueCalibrateButton_clicked] No communication port selected for controller '{}'", controllerName);
            return;
        }
        
        // Send calibration command to Arduino
        std::string calibrateMessage = ArduinoProtocol::createCalibrateMessage(pageLength);
        if (!calibrateMessage.empty()) {
            ArduinoProtocol::sendMessage(eventQueue_, communicationPort, calibrateMessage);
            getLogger()->info("[on_glueCalibrateButton_clicked] Sent calibration command to '{}' via '{}': {}", 
                             communicationPort, calibrateMessage);
        } else {
            getLogger()->error("[on_glueCalibrateButton_clicked] Failed to create calibration message");
            return;
        }
        
        // Disable the button temporarily to prevent multiple clicks
        ui->glueCalibrateButton->setEnabled(false);
        ui->glueCalibrateButton->setText("Calibrating...");
        
        // TODO: Re-enable button after calibration completes or times out
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_glueCalibrateButton_clicked] Exception: {}", e.what());
    }
}

// Handle enable controller checkbox state change
void SettingsWindow::on_glueControllerEnabledCheckBox_stateChanged(int state)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        bool enabled = (state == Qt::Checked);
        
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_)) {
            return;
        }
        
        // Save enabled state directly
        glueSettings["controllers"][currentGlueControllerName_]["enabled"] = enabled;
        
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_glueControllerEnabledCheckBox_stateChanged] Failed to save settings to file");
        }
        
        getLogger()->info("[on_glueControllerEnabledCheckBox_stateChanged] Controller '{}' {} ", 
                         currentGlueControllerName_, enabled ? "enabled" : "disabled");
        
        // Send updated controller setup to reflect the new enabled state
        QMetaObject::invokeMethod(this, [this]() {
            sendControllerSetupToActiveController();
        }, Qt::QueuedConnection);
            
    } catch (const std::exception& e) {
        getLogger()->warn("[on_glueControllerEnabledCheckBox_stateChanged] Exception: {}", e.what());
    }
}

// ============================================================================
//  New Glue Controller Field Slots
// ============================================================================

void SettingsWindow::on_glueStartCurrentSpinBox_valueChanged(double value) {
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        // Save the current controller settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (glueSettings.contains("controllers") && glueSettings["controllers"].contains(currentGlueControllerName_)) {
            glueSettings["controllers"][currentGlueControllerName_]["startCurrent"] = value;
            
            // Update the config
            Config* mutableConfig = const_cast<Config*>(config_);
            mutableConfig->updateGlueSettings(glueSettings);
            mutableConfig->saveToFile();
            
            getLogger()->debug("[on_glueStartCurrentSpinBox_valueChanged] Updated start current to {} A for controller {}", 
                             value, currentGlueControllerName_);
            
            // Send updated config to controller
            sendControllerSetupToActiveController();
        }
    } catch (const std::exception& e) {
        getLogger()->error("[on_glueStartCurrentSpinBox_valueChanged] Error: {}", e.what());
    }
}

void SettingsWindow::on_glueStartDurationSpinBox_valueChanged(double value) {
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        // Save the current controller settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (glueSettings.contains("controllers") && glueSettings["controllers"].contains(currentGlueControllerName_)) {
            glueSettings["controllers"][currentGlueControllerName_]["startDurationMS"] = value;
            
            // Update the config
            Config* mutableConfig = const_cast<Config*>(config_);
            mutableConfig->updateGlueSettings(glueSettings);
            mutableConfig->saveToFile();
            
            getLogger()->debug("[on_glueStartDurationSpinBox_valueChanged] Updated start duration to {} ms for controller {}", 
                             value, currentGlueControllerName_);
            
            // Send updated config to controller
            sendControllerSetupToActiveController();
        }
    } catch (const std::exception& e) {
        getLogger()->error("[on_glueStartDurationSpinBox_valueChanged] Error: {}", e.what());
    }
}

void SettingsWindow::on_glueHoldCurrentSpinBox_valueChanged(double value) {
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        // Save the current controller settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (glueSettings.contains("controllers") && glueSettings["controllers"].contains(currentGlueControllerName_)) {
            glueSettings["controllers"][currentGlueControllerName_]["holdCurrent"] = value;
            
            // Update the config
            Config* mutableConfig = const_cast<Config*>(config_);
            mutableConfig->updateGlueSettings(glueSettings);
            mutableConfig->saveToFile();
            
            getLogger()->debug("[on_glueHoldCurrentSpinBox_valueChanged] Updated hold current to {} A for controller {}", 
                             value, currentGlueControllerName_);
            
            // Send updated config to controller
            sendControllerSetupToActiveController();
        }
    } catch (const std::exception& e) {
        getLogger()->error("[on_glueHoldCurrentSpinBox_valueChanged] Error: {}", e.what());
    }
}

void SettingsWindow::on_glueDotSizeComboBox_currentIndexChanged(int index) {
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty()) {
        return;
    }
    
    try {
        QString dotSize = ui->glueDotSizeComboBox->currentText();
        
        // Save the current controller settings
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (glueSettings.contains("controllers") && glueSettings["controllers"].contains(currentGlueControllerName_)) {
            glueSettings["controllers"][currentGlueControllerName_]["dotSize"] = dotSize.toStdString();
            
            // Update the config
            Config* mutableConfig = const_cast<Config*>(config_);
            mutableConfig->updateGlueSettings(glueSettings);
            mutableConfig->saveToFile();
            
            getLogger()->debug("[on_glueDotSizeComboBox_currentIndexChanged] Updated dot size to '{}' for controller {}", 
                             dotSize.toStdString(), currentGlueControllerName_);
            
            // Send updated config to controller
            sendControllerSetupToActiveController();
        }
    } catch (const std::exception& e) {
        getLogger()->error("[on_glueDotSizeComboBox_currentIndexChanged] Error: {}", e.what());
    }
}

// ============================================================================
//  Arduino Protocol Helper Methods
// ============================================================================



// Handle encoder calibration response from Arduino
void SettingsWindow::onGlueEncoderCalibrationResponse(int pulsesPerPage, const std::string& controllerName)
{
    if (isRefreshing_ || !config_ || controllerName.empty()) {
        return;
    }
    
    try {
        // Get the page length that was used for calibration
        int pageLength = ui->gluePageLengthSpinBox->value();
        
        // Calculate encoder resolution: pulses per mm
        double encoderResolution = static_cast<double>(pulsesPerPage) / static_cast<double>(pageLength);
        
        getLogger()->info("[onGlueEncoderCalibrationResponse] Received calibration data for controller '{}': {} pulses per {} mm page", 
                         controllerName, pulsesPerPage, pageLength);
        getLogger()->info("[onGlueEncoderCalibrationResponse] Calculated encoder resolution: {:.6f} pulses per mm", encoderResolution);
        
        // Update the encoder spinbox with the calculated resolution
        ui->glueEncoderSpinBox->setValue(encoderResolution);
        
        // Save the updated encoder resolution to config
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (glueSettings.contains("controllers") && 
            glueSettings["controllers"].contains(controllerName)) {
            
            glueSettings["controllers"][controllerName]["encoder"] = encoderResolution;
            
            Config* mutableConfig = const_cast<Config*>(config_);
            mutableConfig->updateGlueSettings(glueSettings);
            
            // Persist changes to file
            if (!mutableConfig->saveToFile()) {
                getLogger()->warn("[onGlueEncoderCalibrationResponse] Failed to save updated encoder resolution to file");
            } else {
                getLogger()->info("[onGlueEncoderCalibrationResponse] Successfully saved encoder resolution {:.6f} for controller '{}'", 
                                 encoderResolution, controllerName);
            }
        }
        
        // Re-enable the calibrate button and restore original text
        ui->glueCalibrateButton->setEnabled(true);
        ui->glueCalibrateButton->setText("Calibrate Encoder");
        
        // Show success message or notification (optional)
        getLogger()->info("[onGlueEncoderCalibrationResponse] Encoder calibration completed successfully for controller '{}'", controllerName);
        
    } catch (const std::exception& e) {
        getLogger()->warn("[onGlueEncoderCalibrationResponse] Exception: {}", e.what());
        
        // Re-enable the button even if there was an error
        ui->glueCalibrateButton->setEnabled(true);
        ui->glueCalibrateButton->setText("Calibrate Encoder");
    }
}

// Handle plan name text change
void SettingsWindow::on_gluePlanNameLineEdit_textChanged(const QString& text)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    // Get the current plan ID and new name
    std::string currentPlanId = currentGluePlanName_;
    std::string newName = text.trimmed().toStdString();
    
    // Don't do anything if the name didn't actually change
    if (newName.empty() || newName == currentGluePlanName_) {
        return;
    }
    
    try {
        Config* mutableConfig = const_cast<Config*>(config_);
        nlohmann::json glueSettings = mutableConfig->getGlueSettings();
        
        // Check if the current controller and plan exist
        if (glueSettings.contains("controllers") && 
            glueSettings["controllers"].contains(currentGlueControllerName_) &&
            glueSettings["controllers"][currentGlueControllerName_].contains("plans") &&
            glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentPlanId)) {
            
            auto& controller = glueSettings["controllers"][currentGlueControllerName_];
            auto& plans = controller["plans"];
            
            // Check if this is the active plan
            bool isActivePlan = controller.contains("activePlan") && 
                              controller["activePlan"] == currentPlanId;
            
            // Update the plan name in the settings
            plans[currentPlanId]["name"] = newName;
            
            // If this is the active plan, ensure the activePlan reference is set
            if (isActivePlan) {
                controller["activePlan"] = currentPlanId;
                spdlog::debug("Updated active plan reference for controller '{}' to plan '{}'", 
                            currentGlueControllerName_, currentPlanId);
            }
            
            // Save the updated settings
            mutableConfig->updateGlueSettings(glueSettings);
            
            // Update the combo box display
            int currentIndex = ui->gluePlanSelectorComboBox->currentIndex();
            if (currentIndex >= 0) {
                // Format as "[Plan ID]: [Name]" to preserve the ID part
                QString displayText = QString::fromStdString(currentPlanId);
                if (!text.trimmed().isEmpty()) {
                    displayText += ": " + text.trimmed();
                }
                ui->gluePlanSelectorComboBox->setItemText(currentIndex, displayText);
            }
            
            // Save to file
            if (!mutableConfig->saveToFile()) {
                spdlog::warn("Failed to save plan name changes to file");
            } else {
                spdlog::info("Updated plan name for controller '{}', plan '{}' to '{}'", 
                           currentGlueControllerName_, currentPlanId, newName);
                
                // If this is the active plan, explicitly save the active plan reference
                if (isActivePlan) {
                    saveActivePlanForController();
                }
                
                // Send updated controller setup to Arduino
                sendControllerSetupToActiveController();
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error updating plan name: {}", e.what());
    }
}

// Handle sensor offset value change
void SettingsWindow::on_gluePlanSensorOffsetSpinBox_valueChanged(int value)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_) ||
            !glueSettings["controllers"][currentGlueControllerName_].contains("plans") ||
            !glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentGluePlanName_)) {
            return;
        }
        
        // Save sensor offset directly
        glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_]["sensorOffset"] = value;
        
        Config* mutableConfig = const_cast<Config*>(config_);
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[on_gluePlanSensorOffsetSpinBox_valueChanged] Failed to save settings to file");
        } else {
            // Send updated controller setup to Arduino
            sendControllerSetupToActiveController();
        }
        
    } catch (const std::exception& e) {
        getLogger()->warn("[on_gluePlanSensorOffsetSpinBox_valueChanged] Exception: {}", e.what());
    }
}

// Handle glue row cell change
void SettingsWindow::onGlueRowCellChanged(int row, int column)
{
    if (isRefreshing_ || !config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    // Save gun settings (including rows)
    saveCurrentGunSettings();
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

// Save timer settings to config
void SettingsWindow::saveTimersToConfig() {
    if (!config_ || !ui->timersTable) {
        getLogger()->warn("[saveTimersToConfig] Config object or timers table is null. Cannot save timer settings.");
        return;
    }
    
    Config* mutableConfig = const_cast<Config*>(config_);
    getLogger()->info("[saveTimersToConfig] Saving timer settings to config");
    
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

        try {
            // Timer settings are now updated via setTimerSetting above; no need to call updateTimerSettings here.            
            // Save the configuration to file
            if (mutableConfig->saveToFile()) {
                getLogger()->debug("Settings saved to configuration file");
                
            } else {
                getLogger()->warn("[saveTimersToConfig] Failed to save settings to configuration file");
            }
        } catch (const std::exception& e) {
            getLogger()->warn("[saveTimersToConfig] Exception while saving settings: {}", e.what());
        }

    }
}

// Save data file tab settings to config
void SettingsWindow::saveDataFileSettingsToConfig() {
    if (!config_ || !ui) {
        getLogger()->warn("[saveDataFileSettingsToConfig] Config object or UI is null. Cannot save data file settings.");
        return;
    }
    
    Config* mutableConfig = const_cast<Config*>(config_);
    if (!mutableConfig) {
        getLogger()->error("[saveDataFileSettingsToConfig] Failed to get mutable config");
        return;
    }

    Config::DataFileSettings dataFileSettings;
    
    // Get values from UI elements if they exist
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
    
    // Update the config
    mutableConfig->setDataFileSettings(dataFileSettings);
    
    // Save the configuration to file
    if (mutableConfig->saveToFile()) {
        getLogger()->debug("[saveDataFileSettingsToConfig] Data file settings saved to file");
    } else {
        getLogger()->warn("[saveDataFileSettingsToConfig] Failed to save data file settings to file");
    }
}

// Note: saveSettingsToConfig() has been removed in favor of per-tab save handlers

// ============================================================================
//  UI Event Handler Functions
// ============================================================================

// Set up explicit connections for communication tab elements
void SettingsWindow::setupCommunicationTabConnections() {
    // Connect communication selector combo box
    connect(ui->communicationSelectorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onCommunicationSelectorChanged);
    
    // Connect communication type combo box
    connect(ui->communicationTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!isRefreshing_) {
                    saveCurrentCommunicationSettings();
                    updateCommunicationTypeVisibility(ui->communicationTypeComboBox->currentIndex());
                }
            });
    
    // Connect all communication parameter widgets
    auto connectCommunicationWidget = [this](QWidget* widget) {
        if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int) { 
                        if (!isRefreshing_) saveCurrentCommunicationSettings(); 
                    });
        } else if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            connect(lineEdit, &QLineEdit::textChanged,
                    this, [this](const QString&) { 
                        if (!isRefreshing_) saveCurrentCommunicationSettings(); 
                    });
        } else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
            connect(checkBox, &QCheckBox::stateChanged,
                    this, [this](int) { 
                        if (!isRefreshing_) saveCurrentCommunicationSettings(); 
                    });
        } else if (auto spinBox = qobject_cast<QSpinBox*>(widget)) {
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [this](int) { 
                        if (!isRefreshing_) saveCurrentCommunicationSettings(); 
                    });
        } else if (auto doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
            connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { 
                        if (!isRefreshing_) saveCurrentCommunicationSettings(); 
                    });
        }
    };
    
    // Connect all widgets in the communication tab
    QList<QWidget*> communicationWidgets = ui->communicationTab->findChildren<QWidget*>(
        QString(), Qt::FindDirectChildrenOnly);
    
    for (QWidget* widget : communicationWidgets) {
        // Skip non-editable widgets and buttons
        if (qobject_cast<QPushButton*>(widget) || qobject_cast<QLabel*>(widget) || 
            qobject_cast<QGroupBox*>(widget) || qobject_cast<QStackedWidget*>(widget)) {
            continue;
        }
        connectCommunicationWidget(widget);
    }
    
    // Connect RS232 group box widgets and other communication widgets
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (commStack) {
        for (int i = 0; i < commStack->count(); ++i) {
            QWidget* commPage = commStack->widget(i);
            if (!commPage) continue;
            
            // Find and connect offset spin box
            if (QSpinBox* offsetSpinBox = commPage->findChild<QSpinBox*>(QStringLiteral("offsetSpinBox"))) {
                connect(offsetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                        this, [this](int) { 
                            if (!isRefreshing_) saveCurrentCommunicationSettings();
                        });
            }
            
            // Find RS232 group box
            QGroupBox* rs232Group = commPage->findChild<QGroupBox*>("rs232Group");
            if (rs232Group) {
                // Connect all child widgets of RS232 group
                QList<QWidget*> rs232Widgets = rs232Group->findChildren<QWidget*>(
                    QString(), Qt::FindDirectChildrenOnly);
                    
                for (QWidget* widget : rs232Widgets) {
                    if (qobject_cast<QLabel*>(widget)) {
                        continue;  // Skip labels
                    }
                    connectCommunicationWidget(widget);
                }
            }
        }
    }
    
    // Connect the active checkbox separately as it has special handling
    connect(ui->communicationActiveCheckBox, &QCheckBox::checkStateChanged, this, &SettingsWindow::onCommunicationActiveCheckBoxChanged);
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

void SettingsWindow::onCommunicationActiveCheckBoxChanged(int state) {
    // Enable/disable the communication settings based on the active state
    bool isActive = (state == Qt::Checked);
    
    // Save the current communication settings to preserve the active state change
    // This will update the config with the new active state
    saveCurrentCommunicationSettings();
    
    // Get the current page
    QStackedWidget* commStack = findChild<QStackedWidget*>("communicationStackedWidget");
    if (!commStack) {
        return;
    }
    
    QWidget* currentPage = commStack->currentWidget();
    if (!currentPage) {
        return;
    }
    
    // Only update UI elements if we're not in the middle of refreshing
    // and if this checkbox change is for the currently selected communication
    if (!isRefreshing_ && !currentCommunicationName_.empty()) {
        // Find the UI elements in the current page
        QComboBox* typeComboBox = currentPage->findChild<QComboBox*>("communicationTypeComboBox");
        QGroupBox* rs232Group = currentPage->findChild<QGroupBox*>("rs232Group");
        QGroupBox* tcpipGroup = currentPage->findChild<QGroupBox*>("tcpipGroup");
        
        // Enable/disable the type combo box (reuse existing typeComboBox)
        if (typeComboBox) {
            typeComboBox->setEnabled(isActive);
        }
        
        // Enable/disable all groups based on active state
        if (rs232Group) rs232Group->setEnabled(isActive);
        if (tcpipGroup) tcpipGroup->setEnabled(isActive);
    }
    
    // Update the glue communication combo box after config is saved
    // Use QTimer::singleShot to delay the call until after the config save completes
    QTimer::singleShot(0, this, [this]() {
        populateGlueCommunicationComboBox();
    });
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
            
            // Set the active state (block signals to prevent triggering stateChanged when switching tabs)
            if (activeCheckBox && commData.contains("active")) {
                activeCheckBox->blockSignals(true);
                activeCheckBox->setChecked(commData["active"].get<bool>());
                activeCheckBox->blockSignals(false);
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
            
            // Apply the enable/disable state to UI elements based on the active checkbox
            if (activeCheckBox) {
                bool isActive = activeCheckBox->isChecked();
                
                // Find the UI elements that should be enabled/disabled
                QGroupBox* rs232Group = currentPage->findChild<QGroupBox*>("rs232Group");
                QGroupBox* tcpipGroup = currentPage->findChild<QGroupBox*>("tcpipGroup");
                
                // Enable/disable the type combo box (reuse existing typeComboBox)
                if (typeComboBox) {
                    typeComboBox->setEnabled(isActive);
                }
                
                // Enable/disable all groups based on active state
                if (rs232Group) rs232Group->setEnabled(isActive);
                if (tcpipGroup) tcpipGroup->setEnabled(isActive);
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
        try {
            // Timer settings are now updated via setTimerSetting above; no need to call updateTimerSettings here.            
            // Save the configuration to file
            if (mutableConfig->saveToFile()) {
                getLogger()->debug("communication Settings saved to configuration file");
                
            } else {
                getLogger()->warn("Failed to save communication settings to configuration file");
            }
        } catch (const std::exception& e) {
            getLogger()->warn("Exception while saving communication settings: {}", e.what());
        }
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

// Handle communication table cell change
void SettingsWindow::onCommunicationCellChanged(int row, int column)
{
    if (isRefreshing_ || !config_ || currentCommunicationName_.empty()) {
        return;
    }
    
    // Save changes
    saveCurrentCommunicationSettings();
}

// Gun selector slot implementations

void SettingsWindow::on_gunSelectorComboBox_currentIndexChanged(int index)
{
    if (isRefreshing_ || !config_) {
        return;
    }
    
    // Load the gun data for the selected gun index (0-3 for Gun 1-4)
    loadCurrentGunData(index);
}

void SettingsWindow::on_gunEnabledCheckBox_stateChanged(int state)
{
    if (isRefreshing_ || !config_) {
        return;
    }
    
    // Save the enabled state for the currently selected gun
    saveCurrentGunSettings();
    
}


// Helper function to load gun data for the selected gun
void SettingsWindow::loadCurrentGunData(int gunIndex)
{
    if (!config_ || currentGlueControllerName_.empty() || currentGlueControllerName_ == "Select Controller" || 
        currentGluePlanName_.empty() || currentGluePlanName_ == "Select Plan") {
        return;
    }
    
    // Block signals to prevent multiple change events during gun loading
    bool wasBlocked = ui->glueRowsTable->blockSignals(true);
    bool wasCheckboxBlocked = false;
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("controllers") || 
            !glueSettings["controllers"].contains(currentGlueControllerName_) ||
            !glueSettings["controllers"][currentGlueControllerName_].contains("plans") ||
            !glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentGluePlanName_)) {
            // Restore signal blocking state before returning
            ui->glueRowsTable->blockSignals(wasBlocked);
            return;
        }
        
        auto plan = glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_];
        
        // Block checkbox signals while updating
        wasCheckboxBlocked = ui->gunEnabledCheckBox->blockSignals(true);
        
        // Load gun data if it exists
        if (plan.contains("guns") && plan["guns"].is_array() && gunIndex < plan["guns"].size()) {
            auto gun = plan["guns"][gunIndex];
            
            // Set gun enabled state
            bool enabled = gun.value("enabled", true);
            ui->gunEnabledCheckBox->setChecked(enabled);
            
            // Load gun rows into the table
            loadGunRowsIntoTable(gun);
        } else {
            // Default values for new gun
            ui->gunEnabledCheckBox->setChecked(true);
            ui->glueRowsTable->setRowCount(0);
        }
        
        
    } catch (const std::exception& e) {
        getLogger()->error("[loadCurrentGunData] Exception: {}", e.what());
    } catch (...) {
        getLogger()->error("[loadCurrentGunData] Unknown exception");
    }
    
    // Always restore signal blocking states in the right order
    ui->gunEnabledCheckBox->blockSignals(wasCheckboxBlocked);
    ui->glueRowsTable->blockSignals(wasBlocked);
    
    // Debug log to verify signals are properly restored
    if (ui->glueRowsTable->signalsBlocked()) {
        getLogger()->warn("[loadCurrentGunData] WARNING: Table signals are still blocked after gun load!");
    }
}

// Helper function to load gun rows into the table
void SettingsWindow::loadGunRowsIntoTable(const nlohmann::json& gun)
{
    if (!ui->glueRowsTable) {
        getLogger()->error("[loadGunRowsIntoTable] glueRowsTable is null!");
        return;
    }
    
    // Store current signal blocking state
    bool wasBlocked = ui->glueRowsTable->signalsBlocked();
    
    try {
        // Block signals only if they're not already blocked
        if (!wasBlocked) {
            ui->glueRowsTable->blockSignals(true);
        }
        
        // Clear existing rows
        ui->glueRowsTable->setRowCount(0);
        
        if (!gun.contains("rows") || !gun["rows"].is_array()) {
            // Only restore if we changed it
            if (!wasBlocked) {
                ui->glueRowsTable->blockSignals(wasBlocked);
            }
            return;
        }
        
        auto rows = gun["rows"];
        
        // Load rows with signals blocked
        for (size_t i = 0; i < rows.size(); ++i) {
            auto row = rows[i];
            
            // Add a new row
            int rowPos = ui->glueRowsTable->rowCount();
            ui->glueRowsTable->insertRow(rowPos);
            
            // From column
            QTableWidgetItem* fromItem = new QTableWidgetItem(QString::number(row.value("from", 0)));
            ui->glueRowsTable->setItem(rowPos, 0, fromItem);
            
            // To column
            QTableWidgetItem* toItem = new QTableWidgetItem(QString::number(row.value("to", 100)));
            ui->glueRowsTable->setItem(rowPos, 1, toItem);
            
            // Space column
            QTableWidgetItem* spaceItem = new QTableWidgetItem(QString::number(row.value("space", 5.0)));
            ui->glueRowsTable->setItem(rowPos, 2, spaceItem);
        }
    } catch (const std::exception& e) {
        getLogger()->error("[loadGunRowsIntoTable] Exception: {}", e.what());
    } catch (...) {
        getLogger()->error("[loadGunRowsIntoTable] Unknown exception");
    }
    
    // Always restore the original signal blocking state
    ui->glueRowsTable->blockSignals(wasBlocked);
    
    // Debug log to verify signals are properly restored
    if (ui->glueRowsTable->signalsBlocked() != wasBlocked) {
        getLogger()->warn("[loadGunRowsIntoTable] WARNING: Signal blocking state mismatch after loading rows!");
    }
}

// Helper function to save current gun settings
void SettingsWindow::saveCurrentGunSettings()
{
    if (!config_ || currentGlueControllerName_.empty() || currentGluePlanName_.empty()) {
        return;
    }
    
    try {
        int gunIndex = ui->gunSelectorComboBox->currentIndex();
        if (gunIndex < 0 || gunIndex >= 4) {
            return;
        }
        
        Config* mutableConfig = const_cast<Config*>(config_);
        nlohmann::json glueSettings = mutableConfig->getGlueSettings();
        
        // Ensure the structure exists
        if (!glueSettings.contains("controllers")) {
            glueSettings["controllers"] = nlohmann::json::object();
        }
        if (!glueSettings["controllers"].contains(currentGlueControllerName_)) {
            glueSettings["controllers"][currentGlueControllerName_] = nlohmann::json::object();
        }
        if (!glueSettings["controllers"][currentGlueControllerName_].contains("plans")) {
            glueSettings["controllers"][currentGlueControllerName_]["plans"] = nlohmann::json::object();
        }
        if (!glueSettings["controllers"][currentGlueControllerName_]["plans"].contains(currentGluePlanName_)) {
            glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_] = nlohmann::json::object();
        }
        
        auto& plan = glueSettings["controllers"][currentGlueControllerName_]["plans"][currentGluePlanName_];
        
        // Ensure guns array exists
        if (!plan.contains("guns") || !plan["guns"].is_array()) {
            plan["guns"] = nlohmann::json::array();
            // Initialize with 4 empty guns
            for (int i = 0; i < 4; ++i) {
                nlohmann::json gun;
                gun["gunId"] = i + 1;
                gun["enabled"] = true;
                gun["rows"] = nlohmann::json::array();
                plan["guns"].push_back(gun);
            }
        }
        
        // Update the current gun's enabled state
        if (gunIndex < plan["guns"].size()) {
            plan["guns"][gunIndex]["enabled"] = ui->gunEnabledCheckBox->isChecked();
            
            // Save rows from table
            nlohmann::json rows = nlohmann::json::array();
            for (int i = 0; i < ui->glueRowsTable->rowCount(); ++i) {
                nlohmann::json row;
                
                QTableWidgetItem* fromItem = ui->glueRowsTable->item(i, 0);
                QTableWidgetItem* toItem = ui->glueRowsTable->item(i, 1);
                QTableWidgetItem* spaceItem = ui->glueRowsTable->item(i, 2);
                
                if (fromItem && toItem && spaceItem) {
                    row["from"] = fromItem->text().toInt();
                    row["to"] = toItem->text().toInt();
                    row["space"] = spaceItem->text().toDouble();
                    rows.push_back(row);
                }
            }
            plan["guns"][gunIndex]["rows"] = rows;
        }
        
        // Save the updated settings
        mutableConfig->updateGlueSettings(glueSettings);
        
        // Persist changes to file
        if (!mutableConfig->saveToFile()) {
            getLogger()->warn("[saveCurrentGunSettings] Failed to save settings to file");
        }
        
        // Send updated controller setup to Arduino
        sendControllerSetupToActiveController();
        
    } catch (const std::exception& e) {
        getLogger()->error("[saveCurrentGunSettings] Exception: {}", e.what());
    }
}

// Send comprehensive controller setup message to active controller
void SettingsWindow::sendControllerSetupToActiveController()
{
    if (!config_) {
        return;
    }
    
    try {
        nlohmann::json glueSettings = config_->getGlueSettings();
        if (!glueSettings.contains("activeController") || !glueSettings.contains("controllers")) {
            getLogger()->warn("[sendControllerSetupToActiveController] No active controller or controllers found");
            return;
        }
        
        std::string activeControllerName = glueSettings["activeController"];
        if (!glueSettings["controllers"].contains(activeControllerName)) {
            getLogger()->warn("[sendControllerSetupToActiveController] Active controller '{}' not found", activeControllerName);
            return;
        }
        
        auto controller = glueSettings["controllers"][activeControllerName];
        
        // Get controller enabled state
        bool controllerEnabled = controller.value("enabled", true);
        
        // Get communication port
        std::string communicationPort = controller.value("communication", "");
        if (communicationPort.empty()) {
            getLogger()->warn("[sendControllerSetupToActiveController] No communication port for controller '{}'", activeControllerName);
            return;
        }
        
        // Get controller data
        std::string controllerType = controller.value("type", "dots");
        double encoderResolution = controller.value("encoder", 1.0);
        
        // Get active plan
        std::string activePlanName = controller.value("activePlan", "");
        if (activePlanName.empty() || !controller.contains("plans") || !controller["plans"].contains(activePlanName)) {
            getLogger()->warn("[sendControllerSetupToActiveController] No active plan for controller '{}'", activeControllerName);
            return;
        }
        
        auto plan = controller["plans"][activePlanName];
        int sensorOffset = plan.value("sensorOffset", 10);
        
        // Extract gun configurations
        std::vector<std::pair<bool, std::vector<ArduinoProtocol::GlueRow>>> guns;
        
        if (plan.contains("guns") && plan["guns"].is_array()) {
            for (const auto& gun : plan["guns"]) {
                bool enabled = gun.value("enabled", true);
                std::vector<ArduinoProtocol::GlueRow> gunRows;
                
                if (gun.contains("rows") && gun["rows"].is_array()) {
                    for (const auto& row : gun["rows"]) {
                        ArduinoProtocol::GlueRow glueRow;
                        glueRow.from = row.value("from", 0);
                        glueRow.to = row.value("to", 100);
                        glueRow.space = row.value("space", 5.0);
                        gunRows.push_back(glueRow);
                    }
                }
                
                guns.push_back(std::make_pair(enabled, gunRows));
            }
        } else {
            // Fallback: if no guns array, create 4 empty guns
            for (int i = 0; i < 4; ++i) {
                guns.push_back(std::make_pair(true, std::vector<ArduinoProtocol::GlueRow>()));
            }
        }
        
        // Ensure we have exactly 4 guns
        while (guns.size() < 4) {
            guns.push_back(std::make_pair(false, std::vector<ArduinoProtocol::GlueRow>()));
        }
        if (guns.size() > 4) {
            guns.resize(4);
        }
        
        // Create and send comprehensive controller setup message
        // Get the new glue controller fields
        double startCurrent = ui->glueStartCurrentSpinBox->value();
        double startDurationMS = ui->glueStartDurationSpinBox->value();
        double holdCurrent = ui->glueHoldCurrentSpinBox->value();
        std::string dotSize = ui->glueDotSizeComboBox->currentText().toStdString();
        
        // Create the controller setup message with all fields
        std::string setupMessage = ArduinoProtocol::createControllerSetupMessage(
            controllerType, encoderResolution, sensorOffset, controllerEnabled, guns,
            startCurrent, startDurationMS, holdCurrent, dotSize);
            
        if (!setupMessage.empty()) {
            ArduinoProtocol::sendMessage(eventQueue_, communicationPort, setupMessage);
            getLogger()->info("[sendControllerSetupToActiveController] Sent controller setup to '{}' via '{}': {}", 
                             activeControllerName, communicationPort, setupMessage);
        }
        
    } catch (const std::exception& e) {
        getLogger()->error("[sendControllerSetupToActiveController] Exception: {}", e.what());
    }
}
