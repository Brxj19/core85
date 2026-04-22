#include <QApplication>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Core85"));
    app.setApplicationName(QStringLiteral("Core85"));

    MainWindow window;
    window.show();

    return app.exec();
}
