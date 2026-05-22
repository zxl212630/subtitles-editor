cmake_minimum_required(VERSION 3.20)
project(subtitles-editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================
# SDK Paths (可通过 -D 参数覆盖，如: -DQt6_ROOT=/path/to/qt)
# ============================================================
set(Qt6_ROOT "$ENV{HOME}/Tools/Qt/6.5.7" CACHE PATH "Qt6 installation root")
set(QWindowKit_ROOT "$ENV{HOME}/Tools/Qt/QwindowKit/Qt6" CACHE PATH "QWindowKit installation root")
set(FFMPEG_ROOT "$ENV{HOME}/Tools/ffmpeg/8.0" CACHE PATH "FFmpeg installation root")

# ============================================================
# Qt6
# ============================================================
set(CMAKE_PREFIX_PATH "${Qt6_ROOT};${CMAKE_PREFIX_PATH}")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia Svg)

# ============================================================
# QWindowKit (Qt6 build)
# ============================================================
set(CMAKE_PREFIX_PATH "${QWindowKit_ROOT}/lib/cmake;${CMAKE_PREFIX_PATH}")
find_package(QWindowKit REQUIRED COMPONENTS Widgets)

# ============================================================
# FFmpeg
# ============================================================
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib")

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h PATHS "${FFMPEG_INCLUDE_DIR}")

# ============================================================
# Source files
# ============================================================
set(SOURCES
    src/main.cpp
    src/AppWindow.cpp
    src/SubtitleTrack.cpp
    src/AsrServiceBase.cpp
    src/TencentAsrService.cpp
    src/SubtitleExporter.cpp
    src/SubtitleListModel.cpp
    src/SubtitleListDelegate.cpp
    src/VideoPreviewPanel.cpp
    src/SubtitleListPanel.cpp
    src/TimelinePanel.cpp
    src/ConfigManager.cpp src/ThemeManager.cpp src/TranslationManager.cpp src/PaletteSelectors.cpp
    src/AudioTranscoder.cpp
    src/OssUploader.cpp
    src/FFmpegDecoder.cpp
    src/SeekDecoder.cpp
    src/QtAudioOutput.cpp
    src/SoftwareVideoRenderer.cpp
    src/MediaPlayer.cpp
    src/VideoPropertyDialog.cpp
    src/ConfigDialog.cpp
    src/AsrProgressDialog.cpp
    src/AppMessageBox.cpp
    src/SpeakerManagerDialog.cpp
)

set(HEADERS
    include/AppWindow.h
    include/SubtitleItem.h
    include/SubtitleTrack.h
    include/AsrServiceBase.h
    include/TencentAsrService.h
    include/SubtitleExporter.h
    include/SubtitleListModel.h
    include/SubtitleListDelegate.h
    include/VideoPreviewPanel.h
    include/SubtitleListPanel.h
    include/TimelinePanel.h
    include/ConfigManager.h include/ThemeManager.h include/TranslationManager.h include/PaletteSelectors.h
    include/AudioTranscoder.h
    include/OssUploader.h
    include/FFmpegDecoder.h
    include/SeekDecoder.h
    include/QtAudioOutput.h
    include/SoftwareVideoRenderer.h
    include/MediaPlayer.h
    include/VideoPropertyDialog.h
    include/ConfigDialog.h
    include/AsrProgressDialog.h
    include/AppMessageBox.h
    include/SpeakerManagerDialog.h
)

set(RESOURCES
    resources/resources.qrc
)

# ============================================================
# Executable
# ============================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    "${Qt6_ROOT}/include"
    "${QWindowKit_ROOT}/include"
    "${FFMPEG_INCLUDE_DIR}"
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "${Qt6_ROOT}/lib"
    "${FFMPEG_LIBRARY_DIR}"
)

# Qt dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Network
    Qt::Multimedia
    Qt::Svg
)

# QWindowKit dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    QWindowKit::Core
    QWindowKit::Widgets
)

# FFmpeg dependencies
if(AVCODEC_INCLUDE_DIR AND AVFORMAT_INCLUDE_DIR AND AVUTIL_INCLUDE_DIR AND SWRESAMPLE_INCLUDE_DIR)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        avcodec
        avformat
        avutil
        swscale
        swresample
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

# ============================================================
# Translations
# ============================================================
find_package(Qt6 COMPONENTS LinguistTools REQUIRED)
qt_add_translations(${PROJECT_NAME}
    TS_FILES
        translations/zh_CN.ts
        translations/en_US.ts
    RESOURCE_PREFIX "/translations"
)




