#include "haptic_tests.hpp"
#include <iostream>
#include <stdexcept>
using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
    auto patterns=defaultHapticTests();
    for(auto t:patterns){
        for(int cap:{10,18,25})for(int ms=-20;ms<=6020;++ms){
            const auto motor=hapticTestMotor(t,ms,cap);
            CHECK(motor==0||(motor>=10&&motor<=cap));
            if(ms<500||ms>=5500)CHECK(motor==0);
            if(ms>=0&&ms<6000)CHECK(motor==hapticTestMotor(t,(ms/20)*20,cap));
        }
    }
    HapticTest t{HapticPattern::DoubleKnock,22,10,100,60};
    CHECK(hapticTestMotor(t,580,25)==22);CHECK(hapticTestMotor(t,600,25)==0);
    CHECK(hapticTestMotor(t,640,25)==0);CHECK(hapticTestMotor(t,660,25)==22);
    CHECK(hapticTestMotor(t,760,25)==0);
    t.gapMs=0;for(int ms=500;ms<700;ms+=20)CHECK(hapticTestMotor(t,ms,25)==22);
    t={HapticPattern::Layered,24,12,180,0};
    CHECK(hapticTestMotor(t,1980,25)==12);CHECK(hapticTestMotor(t,2000,25)==24);
    CHECK(hapticTestMotor(t,2160,25)==24);CHECK(hapticTestMotor(t,2180,25)==12);
    for(int ms=500;ms<5500;ms+=20)CHECK(hapticTestMotor(t,ms,25)>=12);
    CHECK(hapticTestMotor(t,2000,18)==18);
    t={HapticPattern::Sweep,25,10,1000,0};int previous=0;
    for(int ms=500;ms<1500;ms+=20){int motor=hapticTestMotor(t,ms,25);CHECK(motor>=previous);previous=motor;}
    CHECK(previous==25);CHECK(hapticTestMotor(t,1500,25)==0);
    t={HapticPattern::DoubleKnock,900,-40,90000,-1};
    auto bounded=boundedHapticTest(t);CHECK(bounded.strength==25&&bounded.low==10&&bounded.durationMs==1000&&bounded.gapMs==0);
    t.pattern=HapticPattern::Count;CHECK(hapticTestMotor(t,500,25)==0);
    std::cout<<"PASS: bounded timing, double-knock separation, uninterrupted background fallback, shared preview grid and output ceiling\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
