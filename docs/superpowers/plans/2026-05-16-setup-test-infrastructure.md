# Setup Test Infrastructure (QTest) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Setup QTest infrastructure and implement missing `ConfigManager` methods using TDD.

**Architecture:** Use `enable_testing()` in CMake build system. Create a dedicated `tests/` directory for unit tests. Each test suite will be a standalone executable using `QTEST_MAIN`.

**Tech Stack:** C++, Qt6 (Core, Test), CMake.

---

### Task 1: Initialize Test Build System

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`

- [ ] **Step 1: Enable testing in root CMakeLists.txt**

Append to the end of `/Users/zxl/Projects/cpp/subtitles-editor/CMakeLists.txt`:
```cmake
# ============================================================
# Tests
# ============================================================
enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 2: Create tests/CMakeLists.txt**

Create `/Users/zxl/Projects/cpp/subtitles-editor/tests/CMakeLists.txt`:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(TestConfigManager TestConfigManager.cpp ../src/ConfigManager.cpp)
target_include_directories(TestConfigManager PRIVATE ../include)
target_link_libraries(TestConfigManager PRIVATE Qt::Core Qt::Test)
add_test(NAME TestConfigManager COMMAND TestConfigManager)
```

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "test: initialize test build system"
```

### Task 2: Implement ConfigManager Tests (TDD - Red Phase)

**Files:**
- Create: `tests/TestConfigManager.cpp`

- [ ] **Step 1: Write failing test for ConfigManager**

Create `/Users/zxl/Projects/cpp/subtitles-editor/tests/TestConfigManager.cpp`:
```cpp
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
```

- [ ] **Step 2: Run test to verify it fails to compile**

Run: `mkdir -p build && cd build && cmake .. && make TestConfigManager`
Expected: Compilation error because `setValue` is not defined in `ConfigManager`.

- [ ] **Step 3: Commit**

```bash
git add tests/TestConfigManager.cpp
git commit -m "test: add failing test for ConfigManager::setValue"
```

### Task 3: Implement ConfigManager Methods (TDD - Green Phase)

**Files:**
- Modify: `include/ConfigManager.h`
- Modify: `src/ConfigManager.cpp`

- [ ] **Step 1: Add method declarations to ConfigManager.h**

Modify `/Users/zxl/Projects/cpp/subtitles-editor/include/ConfigManager.h`:
```cpp
// ... existing code ...
  void setValue(const QString &group, const QString &key, const QVariant &value);
  void sync();
  QString theme() const;
  QString language() const;
// ... existing code ...
```

- [ ] **Step 2: Implement methods in ConfigManager.cpp**

Modify `/Users/zxl/Projects/cpp/subtitles-editor/src/ConfigManager.cpp`:
```cpp
void ConfigManager::setValue(const QString &group, const QString &key, const QVariant &value) {
    settings_.beginGroup(group);
    settings_.setValue(key, value);
    settings_.endGroup();
}

void ConfigManager::sync() {
    settings_.sync();
}

QString ConfigManager::theme() const {
    return getString("general", "theme");
}

QString ConfigManager::language() const {
    return getString("general", "language");
}
```

- [ ] **Step 3: Run test to verify it passes**

Run: `cd build && make TestConfigManager && ./tests/TestConfigManager`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add include/ConfigManager.h src/ConfigManager.cpp
git commit -m "feat: implement missing ConfigManager methods"
```
