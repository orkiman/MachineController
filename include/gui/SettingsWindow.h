#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QTableWidget>
#include <memory> // For std::shared_ptr
#include <spdlog/spdlog.h> // Include spdlog

#include "Event.h"
#include "EventQueue.h"
#include "Config.h"
#include "json.hpp"  // Include the nlohmann JSON header
#include "communication/ArduinoProtocol.h"

namespace Ui {
    class SettingsWindow;
}

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config);
    ~SettingsWindow();

    // Fill communication tab fields with values from settings.json
    void fillCommunicationTabFields();
    
    // Fill timers tab fields with values from settings.json
    void fillTimersTabFields();

    // Fill Data File tab fields with values from config
    void fillDataFileTabFields();
    
    // Fill IO tab with inputs and outputs from config
    void fillIOTabFields();
    
    // Fill Glue tab with controllers and plans from config
    void fillGlueTabFields();
    
 public slots:
    // Update input states in the IO tab
    void updateInputStates(const std::unordered_map<std::string, IOChannel>& inputs);
    
    // Load settings from JSON file

    
    // Slot to indicate initial loading is done
    void onInitialLoadComplete();

    // Gun selector slots
    void on_gunSelectorComboBox_currentIndexChanged(int index);
    void on_gunEnabledCheckBox_stateChanged(int state);
    
    // Row button slots
    void on_addGlueRowButton_clicked();
    void on_removeGlueRowButton_clicked();
    
    // Helper functions for gun management
    void loadCurrentGunData(int gunIndex);
    void loadGunRowsIntoTable(const nlohmann::json& gun);
    void saveCurrentGunSettings();
    
    // Timer settings
    void saveTimersToConfig();
    
    // Controller setup message
    void sendControllerSetupToActiveController();

signals:
    // Signal emitted when output override is enabled/disabled
    void outputOverrideStateChanged(bool enabled);
    
    // Signal emitted when an output state is changed via the UI
    void outputStateChanged(const std::unordered_map<std::string, IOChannel>& outputs);

private slots:
    
    void on_overrideOutputsCheckBox_stateChanged(int state);
    
    // Communication related slots
    void onCommunicationActiveCheckBoxChanged(int state);  // Manual connection to prevent double connections
    void on_communicationSendPushButton_clicked();
    
    void on_refreshButton_clicked();
    
    void updateCommunicationTypeVisibility(int index = 0);
    
    // Updates UI elements when a different communication channel is selected
    void onCommunicationSelectorChanged(int index);
    
    // Save current communication settings before switching to another channel
    void saveCurrentCommunicationSettings();
    
    // Individual defaults buttons handlers
    void onCommunicationDefaultsButtonClicked();
    void onTimersDefaultsButtonClicked();
    
    // Communication table cell changed handler
    void onCommunicationCellChanged(int row, int column);
    
    // Glue tab related slots
    void onGlueControllerSelectorChanged(int index);
    void on_addGlueControllerButton_clicked();
    void on_removeGlueControllerButton_clicked();
    void on_glueControllerNameLineEdit_textChanged(const QString& text);
    void on_glueCommunicationComboBox_currentIndexChanged(int index);
    void on_glueTypeComboBox_currentIndexChanged(int index);
    void on_glueEncoderSpinBox_valueChanged(double value);
    void on_gluePageLengthSpinBox_valueChanged(int value);
    void on_glueCalibrateButton_clicked();
    void on_glueControllerEnabledCheckBox_stateChanged(int state);
    void onGlueEncoderCalibrationResponse(int pulsesPerPage, const std::string& controllerName);
    void onGluePlanSelectorChanged(int index);
    void on_addGluePlanButton_clicked();
    void on_removeGluePlanButton_clicked();
    void on_gluePlanNameLineEdit_textChanged(const QString& text);
    void on_gluePlanSensorOffsetSpinBox_valueChanged(int value);
    void onGlueRowCellChanged(int row, int column);
    
    // New glue controller field slots
    void on_glueStartCurrentSpinBox_valueChanged(double value);
    void on_glueStartDurationSpinBox_valueChanged(double value);
    void on_glueHoldCurrentSpinBox_valueChanged(double value);
    void on_glueDotSizeComboBox_currentIndexChanged(int index);

private:
    // Current selected communication channel name
    std::string currentCommunicationName_;
    
    // Current selected glue controller and plan
    std::string currentGlueControllerName_;
    std::string currentGluePlanName_;
    
    // Initialization flag to prevent messages during startup
    bool isInitializing_ = true;
    
    // Helper function to fill fields with default values
    void fillWithDefaults();
    
    // Helper function to parse char settings (STX, ETX) similar to RS232Communication::parseCharSetting
    int parseCharSetting(const nlohmann::json &settings, const std::string &key, int defaultValue);
    
    // Save timers tab settings to config
    void saveDataFileSettingsToConfig();
    
    // Collect and send current output states to Logic
    void sendCurrentOutputStates();
    
    // Handle output checkbox state change
    void handleOutputCheckboxStateChanged(const QString& outputName, int state);
    
    // Glue tab helper methods
    void saveCurrentGlueControllerSettings();
    void saveCurrentControllerWithoutReload();
    void saveActiveGlueController();
    void saveActivePlanForController();
    void onGlueControllerNameChanged();
    void saveCurrentPlanWithoutReload();
    void updateGlueTypeVisibility(int index = 0);
    void populateGlueCommunicationComboBox();
    void addGlueRowToTable(int from, int to, double space);
    
    // UI Event Handler Functions
    void setupCommunicationTabConnections();
    void setupDataFileTabConnections();
    
    // Arduino protocol helper methods
    void sendRunStopToEnabledControllers(bool run);

    Ui::SettingsWindow *ui;
    EventQueue<EventVariant>& eventQueue_;
    const Config* config_; // Pointer to Config object

    bool isRefreshing_ = false; // Flag to prevent marking items as changed during refresh
    QSet<QWidget*> changedWidgets_; // Set of widgets that have been changed
    bool initialLoadComplete_{false}; // Flag to prevent events during initial load

};

#endif // SETTINGSWINDOW_H
