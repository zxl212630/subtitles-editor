#pragma once

#include <QDialog>
#include <QMap>
#include <QMargins>
#include <QString>
#include "SubtitleTrack.h"

class QListWidget;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QComboBox;
class QLabel;
class QListWidgetItem;

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;

class SpeakerManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit SpeakerManagerDialog(SubtitleTrack *track, QWidget *parent = nullptr);
  ~SpeakerManagerDialog() override;

private slots:
  void onSpeakerSelectionChanged();
  void onAddSpeaker();
  void onRemoveSpeaker();
  void onBrowseFolder();
  void onImageFileChanged(const QString &fileName);
  void onDrawModeChanged(int index);
  void onNameChanged(const QString &name);
  void onMarginChanged(int value);
  void onSaveAndApply();
  void onCancel();
  void updateTheme();

private:
  void setupUi();
  void setupTitleBar();
  void populateSpeakerList();
  void populateImageCombo();
  void updatePreviewImage();
  void loadSettings();
  void scanImageFiles();

  SubtitleTrack *track_ = nullptr;
  
  // 临时存储的说话人信息拷贝，便于取消时恢复
  QMap<int, SpeakerInfo> tempSpeakers_;
  QString tempBgFolder_;
  QMargins tempMargins_;

  // 窗口无边框代理
  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

  // 统一设置区组件（顶部）
  QLineEdit *bgFolderEdit_ = nullptr;
  QPushButton *browseFolderBtn_ = nullptr;
  QSpinBox *marginLeftSpin_ = nullptr;
  QSpinBox *marginRightSpin_ = nullptr;
  QSpinBox *marginTopSpin_ = nullptr;
  QSpinBox *marginBottomSpin_ = nullptr;

  // 说话人列表（左侧）
  QListWidget *speakerList_ = nullptr;
  QPushButton *addBtn_ = nullptr;
  QPushButton *removeBtn_ = nullptr;

  // 说话人属性编辑区（右侧）
  QWidget *propertiesWidget_ = nullptr;
  QLineEdit *nameEdit_ = nullptr;
  QComboBox *imageCombo_ = nullptr;
  QComboBox *drawModeCombo_ = nullptr;
  QLabel *previewLabel_ = nullptr;

  // 底部按钮
  QPushButton *saveBtn_ = nullptr;
  QPushButton *cancelBtn_ = nullptr;

  // 扫描到的背景图文件名列表
  QStringList availableImages_;
  
  // 是否正在进行界面加载，以防触发不必要的信号槽
  bool loading_ = false;
};
