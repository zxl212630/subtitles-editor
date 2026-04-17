#include <QApplication>
#include "AppWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Subtitles Editor");

    AppWindow window;
    window.show();

    return app.exec();
}
