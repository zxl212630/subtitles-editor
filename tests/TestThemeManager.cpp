#include <QtTest>
#include <QApplication>
#include "ThemeManager.h"
#include "ConfigManager.h"

class TestThemeManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // ThemeManager uses qApp in applyTheme
        static int argc = 1;
        static char* argv[] = {(char*)"test"};
        if (!qApp) {
            new QApplication(argc, argv);
        }
    }

    void testThemeMigration() {
        // 1. Force config to "light"
        ConfigManager::instance().setValue("", "theme", "light");
        QCOMPARE(ConfigManager::instance().theme(), QString("light"));

        // 2. Apply theme should trigger migration
        ThemeManager::instance().applyTheme();

        // 3. Verify it's now "dark"
        QCOMPARE(ConfigManager::instance().theme(), QString("dark"));
    }

    void testAvailableThemes() {
        QStringList themes = ThemeManager::instance().availableThemes();
        QVERIFY(themes.contains("dark"));
        QVERIFY(themes.contains("oled"));
        QVERIFY(themes.contains("midnight"));
        QVERIFY(!themes.contains("light"));
    }
};

QTEST_MAIN(TestThemeManager)
#include "TestThemeManager.moc"
