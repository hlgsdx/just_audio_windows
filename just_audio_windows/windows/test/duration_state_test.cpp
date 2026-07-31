#include "duration_state.hpp"

#include <gtest/gtest.h>

namespace just_audio_windows {
namespace {

TEST(DurationStateTest, NormalizesWinRtTicksToMicroseconds) {
  DurationState state;

  EXPECT_EQ(state.ObserveTicks(12345670), 1234567);
}

TEST(DurationStateTest, RemainsUnknownUntilDurationIsPositive) {
  DurationState state;

  EXPECT_EQ(state.ObserveTicks(0), std::nullopt);
  EXPECT_EQ(state.ObserveTicks(-10), std::nullopt);
  EXPECT_EQ(state.ObserveTicks(9), std::nullopt);
}

TEST(DurationStateTest, UnknownUpdateDoesNotEraseKnownDuration) {
  DurationState state;

  EXPECT_EQ(state.ObserveTicks(420000000), 42000000);
  EXPECT_EQ(state.ObserveTicks(0), 42000000);
}

TEST(DurationStateTest, ResetStartsASeparateSourceLifecycle) {
  DurationState state;
  ASSERT_EQ(state.ObserveTicks(420000000), 42000000);

  state.Reset();

  EXPECT_EQ(state.KnownMicroseconds(), std::nullopt);
  EXPECT_EQ(state.ObserveTicks(0), std::nullopt);
}

}  // namespace
}  // namespace just_audio_windows
