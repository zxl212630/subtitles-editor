#pragma once

#include <QDialog>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

class VideoPropertyDialog : public QDialog {
  Q_OBJECT

public:
  using Section = QPair<QString, QMap<QString, QString>>;

  explicit VideoPropertyDialog(const QList<Section> &sections,
                               QWidget *parent = nullptr);

private:
  void setupUi();

  QList<Section> m_sections;
};
