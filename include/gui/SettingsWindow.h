#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <memory> // For std::shared_ptr
#include <spdlog/spdlog.h> // Include spdlog

#include "Event.h"
#include "EventQueue.h"
#include "Config.h"
#include "json.hpp"  // Include the nlohmann JSON header

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
    
    // Fill IO tab with inputs and outputs from config
    void fillIOTabFields();
    
 public slots:
    // Update input states in the IO tab
    void updateInputStates(const std::unordered_map<std::string, IOChannel>& inputs);
    
    // Load settings from JSON file
    bool loadSettingsFromJson(const QString& filePath = "config/settings.json");
    
    // Slot to indicate initial loading is done
    void onInitialLoadComplete();

signals:
    // Signal emitted when output override is enabled/disabled
    void outputOverrideStateChanged(bool enabled);
    
    // Signal emitted when an output state is changed via the UI
    void outputStateChanged(const std::unordered_map<std::string, IOChannel>& outputs);

private slots:
    
    void on_overrideOutputsCheckBox_stateChanged(int state);
    
    // Communication related slots
    void on_communicationActiveCheckBox_stateChanged(int state);
    void on_communication1ActiveCheckBox_stateChanged(int state);
    void on_communication2ActiveCheckBox_stateChanged(int state);
    void on_communication3ActiveCheckBox_stateChanged(int state);
    
    void on_communicationSendPushButton_clicked();
    void on_communication1SendPushButton_clicked();
    void on_communication2SendPushButton_clicked();
    
    void on_refreshButton_clicked();
    
    void updateCommunicationTypeVisibility(int index = 0);
    
    // Updates UI elements when a different communication channel is selected
    void onCommunicationSelectorChanged(int index);
    
    // Save current communication settings before switching to another channel
    void saveCurrentCommunicationSettings();
    
    // Individual defaults buttons handlers
    void onCommunicationDefaultsButtonClicked();
    void onTimersDefaultsButtonClicked();

private:
    // Current selected communication channel name
    std::string currentCommunicationName_;
    
    // Helper function to fill fields with default values
    void fillWithDefaults();
    
    // Helper function to parse char settings (STX, ETX) similar to RS232Communication::parseCharSetting
    int parseCharSetting(const nlohmann::json &settings, const std::string &key, int defaultValue);
    
    // Save settings from UI to Config
    bool saveSettingsToConfig();
    
    // Collect and send current output states to Logic
    void sendCurrentOutputStates();
    
    // Handle output checkbox state change
    void handleOutputCheckboxStateChanged(const QString& outputName, int state);
    
    // Mark a field as changed (with light red background)
    void markAsChanged(QWidget* widget);
    
    // Reset all changed field markings
    void resetChangedFields();
    
    // Connect change events for all editable fields
    void connectChangeEvents();

    Ui::SettingsWindow *ui;
    EventQueue<EventVariant>& eventQueue_;
    const Config* config_; // Pointer to Config object

    bool isRefreshing_ = false; // Flag to prevent marking items as changed during refresh
    QSet<QWidget*> changedWidgets_; // Set of widgets that have been changed
    bool initialLoadComplete_{false}; // Flag to prevent events during initial load

};

#endif // SETTINGSWINDOW_H
