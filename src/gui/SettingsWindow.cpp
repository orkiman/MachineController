#include "gui/SettingsWindow.h"
#include "ui_SettingsWindow.h"
#include "Config.h"
#include <QDebug>
#include <QFile>
#include <QDir>

SettingsWindow::SettingsWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config)
    : QDialog(parent),
      ui(new Ui::SettingsWindow),
      eventQueue_(eventQueue),
      config_(&config)
{
    ui->setupUi(this);
    
    // Fill the communication tab fields with default values
    fillCommunicationTabFields();
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
        
        // Port name
        if (comm1.contains("portName")) {
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
        if (comm1.contains("testMessage")) {
            QString testMessage = QString::fromStdString(comm1["testMessage"]);
            ui->commuication1TestDataLineEdit->setText(testMessage);
        }
    }
    
    // Apply settings from JSON for Communication 2
    if (commSettings.contains("communication2")) {
        auto comm2 = commSettings["communication2"];
        
        // Port name
        if (comm2.contains("portName")) {
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
        if (comm2.contains("testMessage")) {
            QString testMessage = QString::fromStdString(comm2["testMessage"]);
            ui->commuication2TestDataLineEdit->setText(testMessage);
        }
    }
    

    
    // Set defaults for STX/ETX if not already set
    if (ui->stx2LineEdit->text().isEmpty()) {
        ui->stx2LineEdit->setText("02"); // Default if not specified
    }
    
    if (ui->etx2LineEdit->text().isEmpty()) {
        ui->etx2LineEdit->setText("03"); // Default if not specified
    }
    
    // Set test message defaults
    ui->commuication1TestDataLineEdit->setText("Test message 1");
    ui->commuication2TestDataLineEdit->setText("Test message 2");
    
    // Emit a message to the event queue that settings were loaded
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Communication settings loaded from JSON"});
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
    // Send a message through the event queue to notify about settings changes
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Settings applied"});
}

void SettingsWindow::on_cancelButton_clicked() {
    qDebug() << "Cancel button clicked";
    close();
}

void SettingsWindow::on_defaultsButton_clicked() {
    qDebug() << "Defaults button clicked";
    fillWithDefaults();
}

void SettingsWindow::fillWithDefaults() {
    // Fill Communication 1 Tab with default values
    ui->portName1ComboBox->setCurrentText("COM1");
    ui->baudRate1ComboBox->setCurrentText("115200");
    ui->parity1ComboBox->setCurrentText("None");
    ui->dataBits1ComboBox->setCurrentText("8");
    ui->stopBits1ComboBox->setCurrentText("1");
    ui->stx1LineEdit->setText("02");
    ui->etx1LineEdit->setText("03");
    
    // Fill Communication 2 Tab with default values
    ui->portName2ComboBox->setCurrentText("COM2");
    ui->baudRate2ComboBox->setCurrentText("115200");
    ui->parity2ComboBox->setCurrentText("None");
    ui->dataBits2ComboBox->setCurrentText("8");
    ui->stopBits2ComboBox->setCurrentText("1");
    ui->stx2LineEdit->setText("02");
    ui->etx2LineEdit->setText("03");
    
    // Set test message defaults
    ui->commuication1TestDataLineEdit->setText("Test Message 1");
    ui->commuication2TestDataLineEdit->setText("Test Message 2");
    
    // Notify that default settings were loaded
    eventQueue_.push(GuiEvent{GuiEventType::GuiMessage, "Default communication settings loaded", "settings"});
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
