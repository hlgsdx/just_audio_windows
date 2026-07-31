#ifndef JUST_AUDIO_WINDOWS_PLATFORM_TASK_RUNNER_HPP_
#define JUST_AUDIO_WINDOWS_PLATFORM_TASK_RUNNER_HPP_

#include <flutter/plugin_registrar_windows.h>

#include <windows.h>

#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

namespace just_audio_windows {

// Serializes native callback work onto Flutter's Windows platform thread.
//
// Flutter 3.38.10 exposes its platform window-procedure boundary through
// PluginRegistrarWindows. Posting one private window message for the queue
// avoids invoking EventSink from WinRT callback threads and preserves FIFO
// order across playback, data, error, and duration notifications.
class PlatformTaskRunner {
 public:
  explicit PlatformTaskRunner(flutter::PluginRegistrarWindows* registrar)
      : registrar_(registrar) {
    auto* view = registrar_->GetView();
    if (view == nullptr) {
      throw std::runtime_error(
          "just_audio_windows requires an implicit Flutter view");
    }
    view_window_ = view->GetNativeWindow();
    if (view_window_ == nullptr) {
      throw std::runtime_error(
          "just_audio_windows requires a Flutter view window");
    }
    run_tasks_message_ = RegisterWindowMessageW(
        L"LayeredAudio.JustAudioWindows.RunPlatformTasks");
    if (run_tasks_message_ == 0) {
      throw std::runtime_error(
          "just_audio_windows could not register its platform task message");
    }
    delegate_id_ = registrar_->RegisterTopLevelWindowProcDelegate(
        [this](HWND hwnd, UINT message, WPARAM wparam,
               LPARAM lparam) -> std::optional<LRESULT> {
          return HandleWindowMessage(hwnd, message, wparam, lparam);
        });
  }

  ~PlatformTaskRunner() {
    Shutdown();
    registrar_->UnregisterTopLevelWindowProcDelegate(delegate_id_);
  }

  PlatformTaskRunner(const PlatformTaskRunner&) = delete;
  PlatformTaskRunner& operator=(const PlatformTaskRunner&) = delete;

  flutter::BinaryMessenger* messenger() const {
    return registrar_->messenger();
  }

  void Post(std::function<void()> task) {
    bool should_post = false;
    {
      std::scoped_lock lock(mutex_);
      if (shutdown_) {
        return;
      }
      tasks_.push(std::move(task));
      if (!message_pending_) {
        message_pending_ = true;
        should_post = true;
      }
    }

    const HWND target_window = GetAncestor(view_window_, GA_ROOT);
    if (should_post &&
        (target_window == nullptr ||
         !PostMessage(target_window, run_tasks_message_, 0, 0))) {
      std::scoped_lock lock(mutex_);
      message_pending_ = false;
      std::queue<std::function<void()>> empty;
      tasks_.swap(empty);
      std::cerr << "[just_audio_windows] Failed to post platform task: "
                << GetLastError() << std::endl;
    }
  }

  void Shutdown() {
    std::scoped_lock lock(mutex_);
    shutdown_ = true;
    message_pending_ = false;
    std::queue<std::function<void()>> empty;
    tasks_.swap(empty);
  }

 private:
  std::optional<LRESULT> HandleWindowMessage(HWND,
                                              UINT message,
                                              WPARAM,
                                              LPARAM) {
    if (message != run_tasks_message_) {
      return std::nullopt;
    }
    Drain();
    return LRESULT{0};
  }

  void Drain() {
    while (true) {
      std::function<void()> task;
      {
        std::scoped_lock lock(mutex_);
        if (shutdown_ || tasks_.empty()) {
          message_pending_ = false;
          return;
        }
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  flutter::PluginRegistrarWindows* registrar_;
  HWND view_window_ = nullptr;
  UINT run_tasks_message_ = 0;
  int delegate_id_ = 0;
  std::mutex mutex_;
  std::queue<std::function<void()>> tasks_;
  bool message_pending_ = false;
  bool shutdown_ = false;
};

}  // namespace just_audio_windows

#endif  // JUST_AUDIO_WINDOWS_PLATFORM_TASK_RUNNER_HPP_
