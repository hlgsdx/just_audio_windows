#ifndef JUST_AUDIO_WINDOWS_DURATION_STATE_HPP_
#define JUST_AUDIO_WINDOWS_DURATION_STATE_HPP_

#include <cstdint>
#include <mutex>
#include <optional>

namespace just_audio_windows {

// WinRT TimeSpan ticks are 100 nanoseconds; just_audio channel durations are
// integer microseconds. A non-positive native duration means "not known yet".
inline std::optional<int64_t> DurationTicksToMicroseconds(int64_t ticks) {
  if (ticks <= 0) {
    return std::nullopt;
  }
  const auto microseconds = ticks / 10;
  return microseconds > 0 ? std::optional<int64_t>(microseconds)
                          : std::nullopt;
}

// Duration is monotonic-known within one active source. Reset must be called
// before assigning a replacement source.
class DurationState {
 public:
  std::optional<int64_t> ObserveTicks(int64_t ticks) {
    const auto observed = DurationTicksToMicroseconds(ticks);
    std::scoped_lock lock(mutex_);
    if (observed.has_value() && observed.value() > 0) {
      known_microseconds_ = observed;
    }
    return known_microseconds_;
  }

  std::optional<int64_t> KnownMicroseconds() const {
    std::scoped_lock lock(mutex_);
    return known_microseconds_;
  }

  void Reset() {
    std::scoped_lock lock(mutex_);
    known_microseconds_.reset();
  }

 private:
  mutable std::mutex mutex_;
  std::optional<int64_t> known_microseconds_;
};

}  // namespace just_audio_windows

#endif  // JUST_AUDIO_WINDOWS_DURATION_STATE_HPP_
