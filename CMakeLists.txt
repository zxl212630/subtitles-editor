cmake_minimum_required(VERSION 3.20)
project(subtitles-editor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================
# SDK Paths (可通过 -D 参数覆盖，如: -DQt6_ROOT=/path/to/qt)
# ============================================================
if(APPLE)
  set(Qt6_ROOT_DEFAULT "$ENV{HOME}/Tools/Qt/6.5.7")
  set(QWindowKit_ROOT_DEFAULT "$ENV{HOME}/Tools/Qt/QwindowKit/Qt6")
elseif(WIN32)
  set(Qt6_ROOT_DEFAULT "C:/Qt/6.5.7/msvc2019_64")
  set(QWindowKit_ROOT_DEFAULT "C:/Tools/QWindowKit")
else()
  set(Qt6_ROOT_DEFAULT "/usr/lib/qt6")
  set(QWindowKit_ROOT_DEFAULT "/usr")
endif()

set(Qt6_ROOT "${Qt6_ROOT_DEFAULT}" CACHE PATH "Qt6 installation root")
set(QWindowKit_ROOT "${QWindowKit_ROOT_DEFAULT}" CACHE PATH "QWindowKit installation root")

# FFmpeg: 优先使用系统包管理器，可通过 -DFFMPEG_ROOT 覆盖
if(NOT DEFINED FFMPEG_ROOT)
  # 尝试 pkg-config
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET
      libavcodec
      libavformat
      libavutil
      libswscale
      libswresample
    )
  endif()

  if(FFMPEG_FOUND)
    message(STATUS "Found FFmpeg via pkg-config: ${FFMPEG_INCLUDE_DIRS}")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_INCLUDE_DIRS}")
    set(FFMPEG_LIBRARY_DIR "${FFMPEG_LIBRARY_DIRS}")
    set(USE_SYSTEM_FFMPEG TRUE)
  elseif(APPLE)
    # macOS: 直接查找 Homebrew FFmpeg
    if(EXISTS "/opt/homebrew/Cellar/ffmpeg")
      file(GLOB _ffmpeg_versions RELATIVE "/opt/homebrew/Cellar/ffmpeg" "/opt/homebrew/Cellar/ffmpeg/*")
      if(_ffmpeg_versions)
        list(SORT _ffmpeg_versions)
        list(REVERSE _ffmpeg_versions)
        list(GET _ffmpeg_versions 0 _latest_version)
        set(FFMPEG_ROOT "/opt/homebrew/Cellar/ffmpeg/${_latest_version}")
        set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
        set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib")
        set(USE_SYSTEM_FFMPEG TRUE)
        message(STATUS "Found FFmpeg via Homebrew: ${FFMPEG_ROOT}")
      endif()
    endif()
  endif()

  # 回退到手动路径
  if(NOT DEFINED FFMPEG_ROOT)
    if(APPLE)
      set(FFMPEG_ROOT_DEFAULT "$ENV{HOME}/Tools/ffmpeg/8.0")
    elseif(WIN32)
      set(FFMPEG_ROOT_DEFAULT "C:/Tools/ffmpeg")
    else()
      set(FFMPEG_ROOT_DEFAULT "/usr")
    endif()
    set(FFMPEG_ROOT "${FFMPEG_ROOT_DEFAULT}" CACHE PATH "FFmpeg installation root")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib" "${FFMPEG_ROOT}/bin")
  endif()
else()
  # 用户显式指定了 FFMPEG_ROOT
  set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
  set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib" "${FFMPEG_ROOT}/bin")
endif()

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
find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h PATHS "${FFMPEG_INCLUDE_DIR}")
find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h PATHS "${FFMPEG_INCLUDE_DIR}")

# ============================================================
# Source files
# ============================================================
set(SOURCES
    src/BaseDialog.cpp
    src/SubtitleProject.cpp
    src/ProjectManager.cpp
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
    src/CosUploader.cpp
    src/FFmpegDecoder.cpp
    src/SeekDecoder.cpp
    src/QtAudioOutput.cpp
    src/SoftwareVideoRenderer.cpp
    src/MediaPlayer.cpp
    src/VideoPropertyDialog.cpp
    src/ConfigDialog.cpp
    src/AsrProgressDialog.cpp
    src/AsrConfigDialog.cpp
    src/AppMessageBox.cpp
    src/SpeakerManagerDialog.cpp
    src/SubtitleRenderer.cpp
    src/VideoExporter.cpp
    src/ExportDialog.cpp
    src/VideoExportDialog.cpp
)

set(HEADERS
    include/BaseDialog.h
    include/SubtitleProject.h
    include/ProjectManager.h
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
    include/CosUploader.h
    include/FFmpegDecoder.h
    include/SeekDecoder.h
    include/QtAudioOutput.h
    include/SoftwareVideoRenderer.h
    include/MediaPlayer.h
    include/VideoPropertyDialog.h
    include/ConfigDialog.h
    include/AsrProgressDialog.h
    include/AsrConfigDialog.h
    include/AppMessageBox.h
    include/SpeakerManagerDialog.h
    include/SubtitleRenderer.h
    include/VideoExporter.h
    include/ExportDialog.h
    include/VideoExportDialog.h
)

set(RESOURCES
    resources/resources.qrc
)

if(WIN32)
    set(RESOURCES
        ${RESOURCES}
        resources/app.rc
    )
endif()

# ============================================================
# Executable
# ============================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE YES
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    "${Qt6_ROOT}/include"
    "${QWindowKit_ROOT}/include"
    "${FFMPEG_INCLUDE_DIR}"
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "${Qt6_ROOT}/lib"
    ${FFMPEG_LIBRARY_DIR}
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
    if(USE_SYSTEM_FFMPEG)
      # 系统 FFmpeg: 使用 pkg-config 提供的库名
      target_link_libraries(${PROJECT_NAME} PRIVATE
          ${FFMPEG_LIBRARIES}
      )
    else()
      # 手动指定的 FFmpeg
      find_library(AVCODEC_LIB NAMES avcodec libavcodec PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
      find_library(AVFORMAT_LIB NAMES avformat libavformat PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
      find_library(AVUTIL_LIB NAMES avutil libavutil PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
      find_library(SWSCALE_LIB NAMES swscale libswscale PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
      find_library(SWRESAMPLE_LIB NAMES swresample libswresample PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
      target_link_libraries(${PROJECT_NAME} PRIVATE
          ${AVCODEC_LIB}
          ${AVFORMAT_LIB}
          ${AVUTIL_LIB}
          ${SWSCALE_LIB}
          ${SWRESAMPLE_LIB}
      )
    endif()
endif()

# ============================================================
# macOS specific
# ============================================================
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE YES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.subtitles.editor"
        MACOSX_BUNDLE_ICON_NAME "AppIcon"
        MACOSX_BUNDLE_BUNDLE_NAME "Subtitles Editor"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"
    )
    set_source_files_properties(resources/AppIcon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE resources/AppIcon.icns)
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






