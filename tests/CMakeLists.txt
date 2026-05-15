find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(TestConfigManager TestConfigManager.cpp ../src/ConfigManager.cpp)
target_include_directories(TestConfigManager PRIVATE ../include)
target_link_libraries(TestConfigManager PRIVATE Qt::Core Qt::Test)
add_test(NAME TestConfigManager COMMAND TestConfigManager)
