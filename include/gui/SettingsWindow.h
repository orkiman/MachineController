#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
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
    
    // Load settings from JSON file
    bool loadSettingsFromJson(const QString& filePath = "config/settings.json");

private slots:
    void on_overrideOutputsCheckBox_stateChanged(int state);
    void on_applyButton_clicked();
    void on_cancelButton_clicked();
    void on_defaultsButton_clicked();

private:
    // Helper function to fill fields with default values
    void fillWithDefaults();
    
    // Helper function to parse char settings (STX, ETX) similar to RS232Communication::parseCharSetting
    int parseCharSetting(const nlohmann::json &settings, const std::string &key, int defaultValue);

    Ui::SettingsWindow *ui;
    EventQueue<EventVariant>& eventQueue_;
    const Config* config_; // Pointer to Config object
};

#endif // SETTINGSWINDOW_H
