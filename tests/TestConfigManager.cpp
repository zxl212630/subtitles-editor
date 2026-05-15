#include <QtTest>
#include "ConfigManager.h"

class TestConfigManager : public QObject {
    Q_OBJECT
private slots:
    void testSetAndGetValue() {
        ConfigManager::instance().setValue("test_group", "test_key", "hello_world");
        QCOMPARE(ConfigManager::instance().getString("test_group", "test_key"), QString("hello_world"));
    }
};

QTEST_MAIN(TestConfigManager)
#include "TestConfigManager.moc"
