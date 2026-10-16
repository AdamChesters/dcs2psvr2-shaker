#pragma once
#include <array>

namespace shaker {
enum class HapticPattern { Steady, Sweep, Pulse, DoubleKnock, Rhythm, Layered, Count };
inline constexpr int hapticTestDurationMs=6000;
inline constexpr int hapticTestStepMs=20;
inline constexpr std::array<const char*,6> hapticPatternNames={
    "Steady rumble","Sweep","Single pulse","Double knock","Pulsing rumble","Pulse over rumble"};
struct HapticTest {
    HapticPattern pattern=HapticPattern::Steady;
    int strength=18, low=10, durationMs=1000, gapMs=180;
};
std::array<HapticTest,6> defaultHapticTests();
HapticTest boundedHapticTest(HapticTest test);
// Exact requested commands, sampled on the same 20 ms grid as playback.
int hapticTestMotor(HapticTest test,int elapsedMs,int ceiling);
}
