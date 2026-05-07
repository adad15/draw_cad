#include "AutoCadGui.h"

#include <QtWidgets/QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    AutoCadGui window;
    window.resize(980, 720);
    window.show();

    return app.exec();
}
