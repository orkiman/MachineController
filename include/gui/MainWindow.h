#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Event.h"
#include "EventQueue.h"
#include "gui/SettingsWindow.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue);
    ~MainWindow();

private slots:
    void on_runButton_clicked();
    void on_stopButton_clicked();
    void on_settingsButton_clicked();
    void on_clearMessageAreaButton_clicked();
    void on_testButton_clicked();

private:
    Ui::MainWindow *ui;
    EventQueue<EventVariant> &eventQueue_;
    SettingsWindow *settingsWindow_;

};

#endif // MAINWINDOW_H
