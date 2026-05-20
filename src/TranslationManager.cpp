#include "TranslationManager.h"
#include <QApplication>
#include <QDebug>
#include <QTranslator>

TranslationManager &TranslationManager::instance() {
  static TranslationManager instance;
  return instance;
}

TranslationManager::TranslationManager() = default;

void TranslationManager::loadLanguage(const QString &langCode) {
  QString lang = langCode.isEmpty() ? "zh_CN" : langCode;
  if (lang == currentLang_)
    return;

  qDebug() << "[TranslationManager] Switching language to:" << lang;

  // Remove old translator
  if (translator_) {
    qApp->removeTranslator(translator_.get());
  }

  // Create and load new translator
  translator_ = std::make_unique<QTranslator>();
  QString qmPath = QString(":/translations/%1.qm").arg(lang);
  if (translator_->load(qmPath)) {
    qApp->installTranslator(translator_.get());
    currentLang_ = lang;
    emit languageChanged(lang);
  } else {
    qWarning() << "[TranslationManager] Failed to load translation:" << qmPath;
    // Fallback: if loading failed and no current language, try default
    if (currentLang_.isEmpty()) {
      // Try loading default zh_CN as fallback
      if (translator_->load(":/translations/zh_CN.qm")) {
        qApp->installTranslator(translator_.get());
        currentLang_ = "zh_CN";
        emit languageChanged("zh_CN");
      } else {
        qWarning() << "[TranslationManager] No translation available, running "
                      "in source language";
      }
    }
  }
}

QString TranslationManager::currentLanguage() const { return currentLang_; }
