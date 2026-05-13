#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

class VideoPropertyDialog : public QDialog {
  Q_OBJECT

public:
  explicit VideoPropertyDialog(const QMap<QString, QString> &properties,
                               QWidget *parent = nullptr);

private:
  void setupUi();

  QMap<QString, QString> m_properties;
};
