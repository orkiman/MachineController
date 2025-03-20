#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "ui_MainWindow.h"
#include "Event.h"       // Provides full definitions for EventVariant and (if defined there) IOEventType.
#include "EventQueue.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // explicit MainWindow(QWidget *parent = nullptr, EventQueue<EventVariant>& eventQueue);
    explicit MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue);
    ~MainWindow();

private slots:
    // Example slot
    void on_pushButton_clicked();
    void on_o1Button_pressed();
    void on_o1Button_released();

public slots:
    void onUpdateGui(const QString &msg);

private:
    Ui::MainWindow ui;
    EventQueue<EventVariant>& eventQueue_;
};

#endif // MAINWINDOW_H
