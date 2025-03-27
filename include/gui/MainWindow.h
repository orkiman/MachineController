#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_runButton_clicked();
    void on_stopButton_clicked();
    void on_settingsButton_clicked();
    void on_clearMessageAreaButton_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
