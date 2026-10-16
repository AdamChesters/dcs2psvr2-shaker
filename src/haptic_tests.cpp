#include "haptic_tests.hpp"
#include <algorithm>
#include <cmath>

namespace shaker {
std::array<HapticTest,6> defaultHapticTests(){
    return {{{HapticPattern::Steady,14,10,2000,180},
        {HapticPattern::Sweep,20,10,1800,180},
        {HapticPattern::Pulse,20,10,160,180},
        {HapticPattern::DoubleKnock,20,10,160,180},
        {HapticPattern::Rhythm,17,10,200,200},
        {HapticPattern::Layered,22,12,200,180}}};
}
HapticTest boundedHapticTest(HapticTest t){
    t.strength=std::clamp(t.strength,10,25);t.low=std::clamp(t.low,10,t.strength);
    const bool longCue=t.pattern==HapticPattern::Steady || t.pattern==HapticPattern::Sweep;
    t.durationMs=std::clamp(t.durationMs,20,longCue?4000:1000);
    t.gapMs=std::clamp(t.gapMs,0,1500);
    // Controls and playback use whole ticks; no implied sub-tick precision.
    t.durationMs=std::max(20,(t.durationMs/20)*20);t.gapMs=(t.gapMs/20)*20;
    return t;
}
int hapticTestMotor(HapticTest t,int elapsedMs,int ceiling){
    if(elapsedMs<0 || elapsedMs>=hapticTestDurationMs)return 0;
    t=boundedHapticTest(t);const int ms=(elapsedMs/20)*20-500;
    if(ms<0 || ms>=5000)return 0;
    int motor=0;
    switch(t.pattern){
    case HapticPattern::Steady: motor=ms<t.durationMs?t.strength:0;break;
    case HapticPattern::Sweep:
        if(ms<t.durationMs)motor=t.low+int(std::lround(double(t.strength-t.low)*ms/std::max(20,t.durationMs-20)));
        break;
    case HapticPattern::Pulse: motor=ms<t.durationMs?t.strength:0;break;
    case HapticPattern::DoubleKnock:
        motor=(ms<t.durationMs || (ms>=t.durationMs+t.gapMs && ms<2*t.durationMs+t.gapMs))?t.strength:0;break;
    case HapticPattern::Rhythm: motor=ms%(t.durationMs+t.gapMs)<t.durationMs?t.strength:0;break;
    case HapticPattern::Layered: motor=ms>=1500 && ms<1500+t.durationMs?t.strength:t.low;break;
    default: break;
    }
    return motor?std::min(motor,std::clamp(ceiling,10,25)):0;
}
}
