#pragma once

#include <QDialog>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;
class QLabel;

class VideoPropertyDialog : public QDialog {
  Q_OBJECT

public:
  using Section = QPair<QString, QMap<QString, QString>>;

  explicit VideoPropertyDialog(const QList<Section> &sections,
                               QWidget *parent = nullptr);
  ~VideoPropertyDialog() override = default;

private:
  void setupUi();
  void setupTitleBar();

  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

  QList<Section> m_sections;
};
