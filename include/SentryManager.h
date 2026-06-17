#pragma once

class SentryManager {
public:
  static SentryManager &instance();

  void initialize();
  void shutdown();

private:
  SentryManager() = default;
  ~SentryManager() = default;

  bool initialized_ = false;
};
