#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <memory>

// Singleton that manages the QTranslator lifecycle at runtime.
// Replaces the stack-based QTranslator in main() with a heap-allocated one
// that can be swapped when the user changes language in settings.
// Follows the same pattern as ThemeManager.
class TranslationManager : public QObject {
  Q_OBJECT

public:
  static TranslationManager &instance();

  // Load and install translator for the given language code (e.g. "zh_CN",
  // "en_US"). Removes the previous translator if one was installed. Emits
  // languageChanged() after successful installation.
  void loadLanguage(const QString &langCode);

  // Get currently active language code.
  QString currentLanguage() const;

signals:
  // Emitted after a new language is loaded and installed.
  // Widgets should connect to this and call their retranslateUi().
  void languageChanged(const QString &langCode);

private:
  TranslationManager();
  ~TranslationManager() = default;

  std::unique_ptr<QTranslator> translator_;
  QString currentLang_;
};
