#pragma once

#include "BaseDialog.h"
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

class VideoPropertyDialog : public BaseDialog {
  Q_OBJECT

public:
  using Section = QPair<QString, QMap<QString, QString>>;

  explicit VideoPropertyDialog(const QList<Section> &sections,
                               QWidget *parent = nullptr);
  ~VideoPropertyDialog() override = default;

private:
  void setupUi();
  void setupTitleBar();

  QList<Section> m_sections;
};
