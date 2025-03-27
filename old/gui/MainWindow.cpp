#include "gui/MainWindow.h"
#include <QMessageBox>
#include "gui/MainWindow.h"

MainWindow::MainWindow(QWidget *parent, EventQueue<EventVariant>& eventQueue)
    : QMainWindow(parent), eventQueue_(eventQueue)
{
    ui.setupUi(this);
    // Signals and slots connections (optional, usually automatic)
    // connect(ui.pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);
}

MainWindow::~MainWindow() { }

// this is called automatically when the button is clicked
// because When you call ui.setupUi(this), Qt automatically calls QMetaObject::connectSlotsByName(this)
// which connects the signals and slots based on the object names.
void MainWindow::on_pushButton_clicked()
{
    QMessageBox::information(this, "Info", "Start button clicked!");
}
void MainWindow::on_o1Button_pressed()
{
    GuiEvent event;
    event.type = GuiEventType::SetOutput;
    event.uiMessage = "o1Button pressed";
    event.outputName = "o1";
    event.intValue = 1;
    eventQueue_.push(event); // Correctly using the reference directly
}

void MainWindow::on_o1Button_released()
{
    GuiEvent event;
    event.type = GuiEventType::SetOutput;
    event.uiMessage = "o1Button released";
    event.outputName = "o1";
    event.intValue = 0;
    eventQueue_.push(event); // Correctly using the reference directly
}

void MainWindow::onUpdateGui(const QString &msg) {
    ui.massageLable->setText(msg);
}


