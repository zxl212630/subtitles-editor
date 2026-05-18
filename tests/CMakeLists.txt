find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(TestConfigManager TestConfigManager.cpp ../src/ConfigManager.cpp)
target_include_directories(TestConfigManager PRIVATE ../include)
target_link_libraries(TestConfigManager PRIVATE Qt::Core Qt::Test)

add_executable(TestThemeManager TestThemeManager.cpp 
    ../src/ThemeManager.cpp 
    ../src/ConfigManager.cpp
    ../include/ThemeManager.h
    ../resources/resources.qrc
)
target_include_directories(TestThemeManager PRIVATE ../include)
target_link_libraries(TestThemeManager PRIVATE Qt::Core Qt::Gui Qt::Widgets Qt::Test)

add_test(NAME TestConfigManager COMMAND TestConfigManager)
add_test(NAME TestThemeManager COMMAND TestThemeManager)
