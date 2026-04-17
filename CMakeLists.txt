cmake_minimum_required(VERSION 3.20)
project(subtitles-editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================
# SDK Paths
# ============================================================
set(Qt6_ROOT "/Users/zxl/Tools/Qt/6.5.8")
set(QWINDOWKIT_ROOT "/Users/zxl/Tools/Qt/QwindowKit")
set(FFMPEG_ROOT "/Users/zxl/Tools/ffmpeg/8.0")

# ============================================================
# Qt6
# ============================================================
set(CMAKE_PREFIX_PATH "${QT6_ROOT};${CMAKE_PREFIX_PATH}")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ============================================================
# QwindowKit (as Qt plugin or external library)
# ============================================================
#预留QwindowKit集成接口
set(QWINDOWKIT_INCLUDE_DIR "${QWINDOWKIT_ROOT}/include")
set(QWINDOWKIT_LIBRARY "${QWINDOWKIT_ROOT}/lib/${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

# ============================================================
# FFmpeg
# ============================================================
set(FFmpeg_ROOT "${FFMPEG_ROOT}")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib")

# Try finding FFmpeg components
find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h PATHS "${FFMPEG_INCLUDE_DIR}")

# ============================================================
# Source files
# ============================================================
set(SOURCES
    src/main.cpp
    src/AppWindow.cpp
)

set(HEADERS
    include/AppWindow.h
)

# ============================================================
# Executable
# ============================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    "${QT6_ROOT}/include"
    "${QWINDOWKIT_INCLUDE_DIR}"
    "${FFMPEG_INCLUDE_DIR}"
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "${QT6_ROOT}/lib"
    "${QWINDOWKIT_LIBRARY}"
    "${FFMPEG_LIBRARY_DIR}"
)

# Qt dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

# FFmpeg dependencies (placeholders - will be updated when libs are found)
if(AVCODEC_INCLUDE_DIR AND AVFORMAT_INCLUDE_DIR AND AVUTIL_INCLUDE_DIR)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        avcodec
        avformat
        avutil
        swscale
    )
endif()

# ============================================================
# macOS specific
# ============================================================
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE YES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.subtitles.editor"
    )
endif()

# ============================================================
# Install
# ============================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
