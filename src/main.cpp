#include "AppWindow.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("Subtitles Editor");

  // Use Fusion style for better dark theme support and cross-platform
  // consistency
  app.setStyle(QStyleFactory::create("Fusion"));

  // Ensure ConfigManager is initialized after app properties are set
  ConfigManager::instance();

  // Apply theme dynamically using the new ThemeManager
  ThemeManager::instance().applyTheme();

  // Load translation for the configured language
  TranslationManager::instance().loadLanguage(
      ConfigManager::instance().language());

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
