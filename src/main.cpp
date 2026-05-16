#include "AppWindow.h"
#include "ConfigManager.h"
#include <QApplication>
#include <QDebug>
#include <QTranslator>
#include <QStyleFactory>
#include <QPalette>

void setDarkPalette(QApplication &app) {
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(21, 21, 21));
    darkPalette.setColor(QPalette::WindowText, QColor(209, 213, 219));
    darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(21, 21, 21));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, QColor(209, 213, 219));
    darkPalette.setColor(QPalette::Button, QColor(38, 38, 38));
    darkPalette.setColor(QPalette::ButtonText, QColor(209, 213, 219));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(2, 132, 199));

    darkPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    app.setPalette(darkPalette);
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("Subtitles Editor");

  // Use Fusion style for better dark theme support and cross-platform consistency
  app.setStyle(QStyleFactory::create("Fusion"));
  setDarkPalette(app);

  QTranslator translator;
  QString lang = ConfigManager::instance().language();
  if (lang.isEmpty())
    lang = "zh_CN";

  if (translator.load(QString(":/translations/%1.qm").arg(lang))) {
    app.installTranslator(&translator);
  }

  AppWindow window;
  window.show();

  if (argc > 1) {
    QString filePath = QString::fromUtf8(argv[1]);
    qInfo() << "Loading file from command line:" << filePath;
    QMetaObject::invokeMethod(&window, "loadFile", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
  }

  return app.exec();
}
