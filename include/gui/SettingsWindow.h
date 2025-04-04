#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>
#include "Event.h"
#include "EventQueue.h"

namespace Ui {
    class SettingsWindow;
}

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue);
    ~SettingsWindow();

private slots:
    void on_overrideOutputsCheckBox_stateChanged(int state);
    void on_applyButton_clicked();
    void on_cancelButton_clicked();
    void on_defaultsButton_clicked();

private:
    Ui::SettingsWindow *ui;
    EventQueue<EventVariant>& eventQueue_;
};

#endif // SETTINGSWINDOW_H
