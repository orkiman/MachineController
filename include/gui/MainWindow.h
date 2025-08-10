#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <string>
#include <QString>
#include "dataFile/DataFile.h"
#include "Event.h"
#include "EventQueue.h"
#include "gui/SettingsWindow.h"
#include "Config.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

signals:
    // Signal emitted when the window is fully initialized and ready
    void windowReady();

public:
    explicit MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue, const Config& config);
    ~MainWindow();
    

    
    // Getter for the SettingsWindow
    SettingsWindow* getSettingsWindow() const { return settingsWindow_; }
    
    // Add a message to the message area
    void addMessage(const QString& message, const QString& identifier = "");

private slots:
    void on_selectDataFileButton_clicked();

    // Method to signal that the window is fully initialized and ready
    void emitWindowReady();
    
    void on_settingsButton_clicked();
    void on_clearMessageAreaButton_clicked();
    void on_testButton_clicked();

private:
    QString dataFilePath_;
    DataFile dataFile_;

    Ui::MainWindow *ui;
    EventQueue<EventVariant> &eventQueue_;
    SettingsWindow *settingsWindow_;
    const Config* config_;

    // Build and populate the right-side glue test table
    void buildGlueTestTable();

};

#endif // MAINWINDOW_H
