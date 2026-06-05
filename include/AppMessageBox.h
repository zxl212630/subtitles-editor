#pragma once

#include "BaseDialog.h"
#include <QFlags>
#include <QString>

class QLabel;
class QPushButton;
class QFrame;
class QVBoxLayout;

// Frameless, themed replacement for QMessageBox.
// Uses QWindowKit for native frame removal and provides
// the same static API pattern as QMessageBox.
class AppMessageBox : public BaseDialog {
  Q_OBJECT

public:
  enum Icon { NoIcon, Information, Warning, Critical, Question };
  Q_ENUM(Icon)

  // Standard button flags (matching QMessageBox for drop-in compatibility)
  enum StandardButton {
    NoButton = 0x0,
    Ok = 0x1,
    Yes = 0x2,
    No = 0x4,
    Cancel = 0x8
  };
  Q_ENUM(StandardButton)
  Q_DECLARE_FLAGS(StandardButtons, StandardButton)
  Q_FLAGS(StandardButtons)

  // Static convenience methods (mirror QMessageBox API).
  // All return the clicked StandardButton value.
  static int warning(QWidget *parent, const QString &title, const QString &text,
                     StandardButtons buttons = Ok,
                     StandardButton defaultButton = Ok);

  static int question(QWidget *parent, const QString &title,
                      const QString &text, StandardButtons buttons = {Yes, No},
                      StandardButton defaultButton = No);

  static int critical(QWidget *parent, const QString &title,
                      const QString &text, StandardButtons buttons = Ok,
                      StandardButton defaultButton = Ok);

  static int information(QWidget *parent, const QString &title,
                         const QString &text, StandardButtons buttons = Ok,
                         StandardButton defaultButton = Ok);

 private:
  AppMessageBox(Icon icon, const QString &title, const QString &text,
                StandardButtons buttons, StandardButton defaultButton,
                QWidget *parent = nullptr);
  ~AppMessageBox() override = default;

  void setupTitleBar();
  void setupContent();
  void setupFooter();
  void onButtonClicked(StandardButton button);

  QVBoxLayout *mainLayout_ = nullptr;
  QLabel *iconLabel_ = nullptr;
  QLabel *textLabel_ = nullptr;

  QPushButton *okBtn_ = nullptr;
  QPushButton *yesBtn_ = nullptr;
  QPushButton *noBtn_ = nullptr;
  QPushButton *cancelBtn_ = nullptr;

  int clickedButton_ = NoButton;
  StandardButton defaultButton_ = NoButton;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AppMessageBox::StandardButtons)
