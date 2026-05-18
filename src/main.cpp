#include "AppWindow.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include <QApplication>
#include <QDebug>
#include <QTranslator>
#include <QStyleFactory>
#include <QPalette>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("Subtitles Editor");

  // Use Fusion style for better dark theme support and cross-platform consistency
  app.setStyle(QStyleFactory::create("Fusion"));
  
  // Apply theme dynamically using the new ThemeManager
  ThemeManager::instance().applyTheme();

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
