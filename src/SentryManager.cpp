#include "SentryManager.h"
#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <sentry.h>

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <iostream>

#ifndef SENTRY_DSN_DEFAULT
#define SENTRY_DSN_DEFAULT ""
#endif

SentryManager &SentryManager::instance() {
  static SentryManager inst;
  return inst;
}

void SentryManager::initialize() {
  if (initialized_)
    return;

  // Check if Sentry is enabled in config.ini. Default is true.
  bool enabled = ConfigManager::instance().getBool("sentry", "enabled", true);
  if (!enabled) {
    qDebug() << "[Sentry] Disabled by configuration.";
    return;
  }

  // Determine DSN: config.ini takes precedence over build-time default
  QString dsn = ConfigManager::instance().getString("sentry", "dsn");
  if (dsn.isEmpty()) {
    dsn = QString::fromLatin1(SENTRY_DSN_DEFAULT);
  }

  if (dsn.isEmpty() || dsn.contains("YOUR_")) {
    qDebug() << "[Sentry] DSN is empty or invalid. Skipping initialization.";
    return;
  }

  sentry_options_t *options = sentry_options_new();
  sentry_options_set_dsn(options, dsn.toUtf8().constData());

  // Configure database path (where minidumps are cached)
  QString dbPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/sentry-db";
  QDir().mkpath(dbPath);
  sentry_options_set_database_path(
      options, QDir::toNativeSeparators(dbPath).toUtf8().constData());

  // Add redirected log file as an attachment
  QString logPath;
#ifdef Q_OS_MAC
  logPath = "/tmp/startup.log";
#else
  logPath = QCoreApplication::applicationDirPath() + "/startup.log";
  if (!QFile::exists(logPath)) {
    logPath = "startup.log";
  }
#endif

  if (QFile::exists(logPath)) {
    sentry_options_add_attachment(
        options, QDir::toNativeSeparators(logPath).toUtf8().constData());
    qDebug() << "[Sentry] Attached redirected log file:" << logPath;
  } else {
    // Fallback to unified directory log path
    QString fallbackLogPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        "/logs/app.log";
    if (QFile::exists(fallbackLogPath)) {
      sentry_options_add_attachment(
          options,
          QDir::toNativeSeparators(fallbackLogPath).toUtf8().constData());
      qDebug() << "[Sentry] Attached fallback log file:" << fallbackLogPath;
    }
  }

  // Configure crashpad_handler path (expected to be next to the executable)
  QString appDir = QCoreApplication::applicationDirPath();
  QString handlerPath = appDir + "/crashpad_handler";
#ifdef Q_OS_WIN
  handlerPath += ".exe";
#endif

  if (QFile::exists(handlerPath)) {
    sentry_options_set_handler_path(
        options, QDir::toNativeSeparators(handlerPath).toUtf8().constData());
    qDebug() << "[Sentry] Using crashpad_handler at:" << handlerPath;
  } else {
    qWarning() << "[Sentry] crashpad_handler not found at:" << handlerPath
               << ". Crash reporting might fail.";
  }

  // Set Release info
  QString release =
      QString("%1@%2")
          .arg(QCoreApplication::applicationName().replace(" ", ""))
          .arg(QCoreApplication::applicationVersion());
  sentry_options_set_release(options, release.toUtf8().constData());

  // Set environment (dev or production)
#ifdef QT_DEBUG
  sentry_options_set_environment(options, "development");
#else
  sentry_options_set_environment(options, "production");
#endif

  int ret = sentry_init(options);
  if (ret == 0) {
    initialized_ = true;
    qDebug() << "[Sentry] Initialized successfully. DSN:" << dsn
             << "Release:" << release;
  } else {
    qCritical() << "[Sentry] Initialization failed with code:" << ret;
  }
}

void SentryManager::shutdown() {
  if (!initialized_)
    return;
  sentry_close();
  initialized_ = false;
  qDebug() << "[Sentry] Closed successfully.";
}
