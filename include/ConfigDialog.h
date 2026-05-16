#pragma once
#include <QDialog>
#include <QVariantMap>

class QListWidget;
class QStackedWidget;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog() override = default;

private slots:
    void onApply();
    void onOk();
    void onCancel();
    void checkDirtyState();

private:
    void setupUi();
    void setupTitleBar();
    void loadConfig();
    void saveConfig();
    bool isDirty() const;

    QWK::WidgetWindowAgent *windowAgent = nullptr;
    QFrame *titleBar = nullptr;
    QLabel *titleLabel = nullptr;

    QListWidget *sidebarList_;
    QStackedWidget *stackedWidget_;
    
    // General Page
    QComboBox *langCombo_;
    QComboBox *themeCombo_;

    // Storage Page
    QComboBox *storageProviderCombo_;
    QLineEdit *ossBucketEdit_;
    QLineEdit *ossRegionEdit_;
    QLineEdit *ossAccessKeyEdit_;
    QLineEdit *ossSecretKeyEdit_;

    // ASR Page
    QComboBox *asrProviderCombo_;
    QLineEdit *tencentAppIdEdit_;
    QLineEdit *tencentSecretIdEdit_;
    QLineEdit *tencentSecretKeyEdit_;

    // Footer
    QLabel *dirtyLabel_;
    QPushButton *btnApply_;
    QPushButton *btnOk_;
    QPushButton *btnCancel_;

    QVariantMap initialConfig_;
};
