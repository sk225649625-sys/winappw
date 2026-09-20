#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("ReelForge");
    app.setOrganizationName("ReelForge");

    rf::MainWindow win;
    win.show();
    return app.exec();
}
