#include "AppWindow.h"
#include "ConfigManager.h"
#include "MediaPlayer.h"
#include "SoftwareVideoRenderer.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include "VideoExporter.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QEvent>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <cmath>

class QWinIdCrashBypasser : public QObject {
public:
  static QWinIdCrashBypasser *instance() {
    static QWinIdCrashBypasser inst;
    return &inst;
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event && event->type() == QEvent::WinIdChange) {
      if (watched && (watched->inherits("QOpenGLWidget") ||
                      watched->inherits("HardwareVideoRenderer"))) {
        return true; // 拦截事件，避免 QWindowKit 崩溃
      }
    }
    return QObject::eventFilter(watched, event);
  }
};

int main(int argc, char *argv[]) {
  // Check for --benchmark flag
  bool runBenchmark = false;
  QString benchmarkVideoPath;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
      runBenchmark = true;
      benchmarkVideoPath = QString::fromUtf8(argv[i + 1]);
      break;
    }
  }

  if (runBenchmark) {
    QApplication app(argc, argv);
    app.installEventFilter(QWinIdCrashBypasser::instance());

    // Set up MediaPlayer and a dummy SoftwareVideoRenderer to receive frames
    MediaPlayer player;
    SoftwareVideoRenderer renderer;
    player.setVideoRenderer(&renderer);

    qInfo() << "[BENCHMARK] Starting video preview benchmark on file:"
            << benchmarkVideoPath;

    struct Scenario {
      QString name;
      int intervalMs;
      int totalSteps;
    };

    struct ScenarioStats {
      int seekRequests = 0;
      int framesRendered = 0;
      double totalErrorMs = 0;
      int errorCount = 0;
      qint64 startTime = 0;
      qint64 endTime = 0;
    };

    struct BenchmarkRunner {
      QList<Scenario> scenarios;
      QList<ScenarioStats> stats;
      int currentScenarioIdx = 0;
      int currentStep = 0;
      QTimer *dragTimer = nullptr;
      qint64 durationMs = 0;
    } *runner = new BenchmarkRunner();

    runner->scenarios = {
        {"Slow Forward Drag (40ms interval, 100ms step, sequential)", 40, 50},
        {"Fast Forward Drag (10ms interval, 100ms step, highly sequential)", 10,
         100},
        {"Backward Drag (25ms interval, -200ms step, sequential-like)", 25, 50},
        {"Large Random Jumps (200ms interval, non-sequential timeline clicks)",
         200, 20},
        {"Within GOP (Small span: 500ms jump, sequential path)", 100, 10},
        {"Within GOP (Medium span: 4s jump, full seek path)", 300, 10},
        {"Across Few GOPs (Large span: 15s jump, 1-2 GOP hops)", 400, 8},
        {"Across Many GOPs (Huge span: 50s jump, 5+ GOP hops)", 500, 3}};
    runner->stats.resize(runner->scenarios.size());

    QObject::connect(
        &player, &MediaPlayer::previewFrameRendered,
        [runner](qint64 ptsMs, qint64 targetMs) {
          if (runner->currentScenarioIdx >= runner->scenarios.size())
            return;
          auto &s = runner->stats[runner->currentScenarioIdx];
          s.framesRendered++;
          qint64 error = qAbs(ptsMs - targetMs);
          s.totalErrorMs += error;
          s.errorCount++;
          qInfo() << QString("[%1] Rendered preview frame at %2 ms for target "
                             "%3 ms, error: %4 ms")
                         .arg(
                             runner->scenarios[runner->currentScenarioIdx].name)
                         .arg(ptsMs)
                         .arg(targetMs)
                         .arg(error);
        });

    QObject::connect(
        &player, &MediaPlayer::mediaLoaded,
        [&player, runner, benchmarkVideoPath](qint64 durationMs, QSize size) {
          qInfo() << "[BENCHMARK] Media loaded successfully. Duration:"
                  << durationMs << "ms, Size:" << size.width() << "x"
                  << size.height();
          runner->durationMs = durationMs;

          runner->dragTimer = new QTimer(&player);

          auto runNextScenario = [&player, runner, benchmarkVideoPath]() {
            if (runner->currentScenarioIdx >= runner->scenarios.size()) {
              runner->dragTimer->stop();
              qInfo() << "[BENCHMARK] All scenarios complete. Stopping preview "
                         "dragging.";
              player.stopPreviewDragging();

              // Wait 500ms for final precise seek to complete, then print
              // report
              QTimer::singleShot(500, [runner, benchmarkVideoPath]() {
                qInfo() << "\n================================================="
                           "=================";
                qInfo() << "                 VIDEO PREVIEW BENCHMARK REPORT";
                qInfo() << "==================================================="
                           "===============";

                for (int i = 0; i < runner->scenarios.size(); ++i) {
                  const auto &scen = runner->scenarios[i];
                  const auto &st = runner->stats[i];
                  qint64 duration = st.endTime - st.startTime;
                  qInfo()
                      << QString("Scenario %1: %2").arg(i + 1).arg(scen.name);
                  qInfo() << QString("  - Seeks Requested: %1")
                                 .arg(st.seekRequests);
                  qInfo() << QString("  - Frames Rendered: %1")
                                 .arg(st.framesRendered);
                  if (st.seekRequests > 0) {
                    double renderRate =
                        (st.framesRendered * 100.0) / st.seekRequests;
                    qInfo() << QString("  - Render/Coalesce Success Rate: %1%")
                                   .arg(renderRate, 0, 'f', 1);
                  }
                  if (st.errorCount > 0) {
                    qInfo()
                        << QString("  - Average Timestamp Error: %1 ms")
                               .arg(st.totalErrorMs / st.errorCount, 0, 'f', 2);
                  } else {
                    qInfo() << "  - Average Timestamp Error: N/A";
                  }
                  qInfo() << QString("  - Execution Time: %1 ms").arg(duration);
                  qInfo() << "-------------------------------------------------"
                             "-----------------";
                }

                qInfo() << "\n>>> Starting Video Exporter Benchmark (Hardware "
                           "VideoToolbox)...";
                VideoExporter *exporter = new VideoExporter();
                VideoExportConfig config;
                config.inputPath = benchmarkVideoPath;
                config.outputPath = "/tmp/benchmark_export.mp4";
#ifdef Q_OS_MAC
                config.videoCodec = "h264_videotoolbox";
#else
                config.videoCodec = "libx264";
#endif
                config.qualityMode = VideoExportConfig::QualityMedium;
                config.exportAudio = false; // testsrc has no audio
                exporter->setConfig(config);

                static bool exportSuccess = false;

                QObject::connect(
                    exporter, &VideoExporter::progressChanged,
                    QCoreApplication::instance(), [](int percent) {
                      qInfo() << QString("[EXPORT] Progress: %1%").arg(percent);
                    });

                QObject::connect(
                    exporter, &VideoExporter::exportFinished,
                    QCoreApplication::instance(), [](const QString &out) {
                      qInfo() << "[EXPORT] Successfully finished exporting to:"
                              << out;
                      exportSuccess = true;
                    });

                QObject::connect(
                    exporter, &VideoExporter::exportFailed,
                    QCoreApplication::instance(), [](const QString &err) {
                      qCritical() << "[EXPORT] Export failed:" << err;
                      exportSuccess = false;
                    });

                QObject::connect(exporter, &VideoExporter::finished,
                                 QCoreApplication::instance(), [exporter]() {
                                   qInfo() << "[EXPORT] Thread finished "
                                              "safely. Exiting benchmark.";
                                   exporter->deleteLater();
                                   if (exportSuccess) {
                                     QCoreApplication::quit();
                                   } else {
                                     QCoreApplication::exit(1);
                                   }
                                 });

                exporter->start();
              });
              return;
            }

            auto &scen = runner->scenarios[runner->currentScenarioIdx];
            auto &st = runner->stats[runner->currentScenarioIdx];
            qInfo() << QString("\n>>> Starting Scenario %1: %2")
                           .arg(runner->currentScenarioIdx + 1)
                           .arg(scen.name);
            st.startTime = QDateTime::currentMSecsSinceEpoch();
            runner->currentStep = 0;
            runner->dragTimer->setInterval(scen.intervalMs);
            runner->dragTimer->start();
          };

          QObject::connect(
              runner->dragTimer, &QTimer::timeout,
              [&player, runner, runNextScenario]() {
                if (runner->currentScenarioIdx >= runner->scenarios.size())
                  return;
                const auto &scen =
                    runner->scenarios[runner->currentScenarioIdx];
                auto &st = runner->stats[runner->currentScenarioIdx];

                if (runner->currentStep >= scen.totalSteps) {
                  st.endTime = QDateTime::currentMSecsSinceEpoch();
                  runner->dragTimer->stop();
                  qInfo() << QString(">>> Scenario %1 finished.")
                                 .arg(runner->currentScenarioIdx + 1);
                  runner->currentScenarioIdx++;
                  // Wait 100ms before starting next scenario to let current
                  // decoder cool down
                  QTimer::singleShot(100, runNextScenario);
                  return;
                }

                // Calculate targetMs depending on currentScenarioIdx
                qint64 targetMs = 0;
                if (runner->currentScenarioIdx == 0) {
                  // Slow Forward Drag: 0 to 5s in steps of 100ms
                  targetMs = runner->currentStep * 100;
                } else if (runner->currentScenarioIdx == 1) {
                  // Fast Forward Drag: 5s to 15s in steps of 100ms
                  targetMs = 5000 + runner->currentStep * 100;
                } else if (runner->currentScenarioIdx == 2) {
                  // Backward Drag: 15s down to 5s in steps of -200ms
                  targetMs = 15000 - runner->currentStep * 200;
                } else if (runner->currentScenarioIdx == 3) {
                  // Large Random Jumps: click at 0%, 25%, 50%, 75%, etc.
                  double ratio = (runner->currentStep % 4) * 0.25;
                  targetMs =
                      static_cast<qint64>(ratio * runner->durationMs *
                                          0.9); // keep within 90% of duration
                } else if (runner->currentScenarioIdx == 4) {
                  // Within GOP (Small span): jump forward by 500ms each step
                  // (0.5s is < 2s, sequential)
                  targetMs = runner->currentStep * 500;
                } else if (runner->currentScenarioIdx == 5) {
                  // Within GOP (Medium span): jump forward by 4000ms each step
                  // (4s is > 2s so full seek, but in same/adjacent GOP)
                  targetMs = runner->currentStep * 4000;
                } else if (runner->currentScenarioIdx == 6) {
                  // Across Few GOPs: jump forward by 15000ms each step (15s
                  // = 1.5 GOPs)
                  targetMs = runner->currentStep * 15000;
                } else if (runner->currentScenarioIdx == 7) {
                  // Across Many GOPs: jump forward by 50000ms each step (50s =
                  // 5 GOPs)
                  targetMs = runner->currentStep * 50000;
                }

                st.seekRequests++;
                player.previewSeek(targetMs);
                runner->currentStep++;
              });

          runNextScenario();
        });

    QObject::connect(&player, &MediaPlayer::playbackError,
                     [](const QString &err) {
                       qCritical() << "[BENCHMARK] Error loading media:" << err;
                       QCoreApplication::exit(1);
                     });

    player.load(benchmarkVideoPath);
    return app.exec();
  }

  QApplication app(argc, argv);
  app.installEventFilter(QWinIdCrashBypasser::instance());
  app.setApplicationName("Subtitles Editor");
  app.setWindowIcon(QIcon(":/icon.png"));

  // Use Fusion style for better dark theme support and cross-platform
  // consistency
  app.setStyle(QStyleFactory::create("Fusion"));

  // Ensure ConfigManager is initialized after app properties are set
  ConfigManager::instance();

  // Apply theme dynamically using the new ThemeManager
  ThemeManager::instance().applyTheme();

  // Load translation for the configured language
  TranslationManager::instance().loadLanguage(
      ConfigManager::instance().language());

  AppWindow window;
  window.show();

  if (argc > 1) {
    QString filePath = QString::fromUtf8(argv[1]);
    qInfo() << "Loading file from command line:" << filePath;
    QMetaObject::invokeMethod(&window, "loadFile", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
  }

  return app.exec();
}
