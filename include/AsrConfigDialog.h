#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;

class AsrConfigDialog : public QDialog {
  Q_OBJECT
public:
  explicit AsrConfigDialog(QWidget *parent = nullptr);
  ~AsrConfigDialog() override = default;

  QString engineModelType() const;
  int sentenceMaxLength() const;
  bool speakerDiarization() const;

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUi();
  void setupTitleBar();
  void retranslateUi();
  void loadDefaultConfig();

  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

  QSpinBox *sentenceMaxLengthSpin_ = nullptr;
  QComboBox *engineModelTypeCombo_ = nullptr;
  QCheckBox *speakerDiarizationCheck_ = nullptr;

  QLabel *maxLenLabel_ = nullptr;
  QLabel *engineLabel_ = nullptr;
  QLabel *speakerDiarizationLabel_ = nullptr;

  QPushButton *btnOk_ = nullptr;
  QPushButton *btnCancel_ = nullptr;
};
