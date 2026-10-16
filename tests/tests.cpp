#include "core.hpp"
#ifndef SHAKER_CORE_ONLY
#include "integration.hpp"
#endif
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("Check failed: ")+ #x +" at line "+std::to_string(__LINE__));}while(0)
static Frame frame(double t,uint64_t sequence=1){Settings s;auto f=flightDemoFrame(t,1000,flightDemoStages(s),s.gearDemoSeconds);f.sequence=sequence;f.values["on_ground"]=0;f.values["gear"]=0;f.values["shake"]=0;return f;}
static void flightFeedTests(){
    FlightFeed feed;CHECK(!feed.status(1000,300).telemetryLive);
    auto f=frame(1);feed.observe(f,1000);CHECK(!feed.status(1000,300).telemetryLive);
    f.sequence++;f.simTime+=.02;feed.observe(f,1020);auto status=feed.status(1020,300);
    CHECK(status.telemetryLive&&status.aircraftLive&&status.supported);
    CHECK(!feed.status(1010,300).telemetryLive); // A future publisher timestamp cannot look fresh.
    feed.observe(f,1319);CHECK(feed.status(1319,300).ageMs==299); // Duplicate frames cannot refresh the clock.
    CHECK(!feed.status(1320,300).telemetryLive);
    f.sequence++;feed.observe(f,1320);CHECK(!feed.status(1320,300).telemetryLive); // New packets with paused time stay stale.
    f.sequence++;f.simTime+=.02;f.state="no_aircraft";f.aircraft.clear();f.values.clear();feed.observe(f,1340);
    status=feed.status(1340,300);CHECK(status.telemetryLive&&!status.aircraftLive&&!status.supported);
    f.sequence++;f.simTime+=.02;f.state="flying";f.aircraft="FA-18C_hornet";feed.observe(f,1360);
    CHECK(!feed.status(1360,300).aircraftLive); // Identity alone is not aircraft telemetry.
    f.sequence++;f.simTime+=.02;f.values["gear"]=0;f.aircraft="F-16C_50";feed.observe(f,1380);
    status=feed.status(1380,300);CHECK(status.telemetryLive&&status.aircraftLive&&status.supported);
    f.sequence++;f.state="stopped";feed.observe(f,1400);CHECK(!feed.status(1400,300).telemetryLive);
    f=frame(0);f.session=99;feed.observe(f,1420);CHECK(!feed.status(1420,300).telemetryLive);
    f.sequence++;f.simTime=.02;feed.observe(f,1440);CHECK(feed.status(1440,300).supported);
    f.sequence++;f.simTime=0;feed.observe(f,1460);CHECK(!feed.status(1460,300).telemetryLive);
    f.sequence++;f.simTime=.02;feed.observe(f,1480);CHECK(feed.status(1480,300).supported);
    // Synthetic engine input never passes through the real-feed tracker.
    EffectEngine demo;Settings s;demo.ingest(effectDemoFrame(Gun,1,2000),2000);
    CHECK(!feed.status(2000,300).telemetryLive);feed.reset();CHECK(feed.status(2000,300).aircraft.empty());
}
static void flightDemoTests(){
    for(float travel:{4.8f,12.1f,15.f}){
        Settings s;s.gearDemoSeconds=travel;const auto stages=flightDemoStages(s);
        CHECK(stages.size()==11);CHECK(flightDemoStageAt(stages,-1)==-1);
        for(size_t i=0;i<stages.size();++i){
            CHECK(stages[i].duration>0);CHECK(flightDemoStageAt(stages,stages[i].start)==int(i));
            if(i)CHECK(stages[i].start==stages[i-1].start+stages[i-1].duration);
        }
        const auto duration=stages.back().start+stages.back().duration;CHECK(flightDemoStageAt(stages,duration)==-1);
        EffectEngine engine;std::array<bool,effectCount> felt{};
        int gearStarts=0,gearLocks=0;bool inStart=false,inLock=false;
        for(int n=0;n*.02<duration;++n){
            const double t=n*.02;const auto now=uint64_t(50000+n*20);
            auto f=flightDemoFrame(t,now,stages,travel);engine.ingest(f,now);const auto mix=engine.tick(now,s);
            const auto& stage=stages.at(size_t(flightDemoStageAt(stages,t)));
            CHECK(mix.motor>=0&&mix.motor<=s.motorCap);
            if(stage.step==FlightDemoStep::Roll||stage.step==FlightDemoStep::Rest)CHECK(mix.motor==0);
            if(mix.motor>0&&mix.dominant>=0)felt[size_t(mix.dominant)]=true;
            bool start=mix.effects[Gear].reason=="Start THUNK",lock=mix.effects[Gear].reason=="Lock THUNK";
            if(start&&!inStart)++gearStarts;if(lock&&!inLock)++gearLocks;inStart=start;inLock=lock;
            if(start||lock)CHECK(stage.step==FlightDemoStep::GearUp||stage.step==FlightDemoStep::GearDown);
            if(mix.dominant==Gear&&mix.effects[Gear].reason.find("quiet")!=std::string::npos)CHECK(mix.motor==0);
        }
        CHECK(gearStarts==2&&gearLocks==2);
        for(size_t i:{size_t(Gear),size_t(Gun),size_t(Buffet),size_t(Airflow),size_t(Afterburner),size_t(AfterburnerRumble),size_t(Stores),size_t(Countermeasures),size_t(Touchdown)})CHECK(felt[i]);
        CHECK(!felt[Engine]&&!felt[Taxi]&&!felt[Damage]);CHECK(engine.tick(50000+uint64_t(duration*1000)+400,s).motor==0);
        for(auto& cue:s.effects)cue.enabled=false;engine.reset();
        for(int n=0;n*.02<duration;++n){const auto now=uint64_t(150000+n*20);engine.ingest(flightDemoFrame(n*.02,now,stages,travel),now);CHECK(engine.tick(now,s).motor==0);}
    }
}
static void parseTests(){
    Frame f;std::string error;
    CHECK(!parseFrame("{",f,error));CHECK(!parseFrame(std::string(32769,'x'),f,error));
    CHECK(!parseFrame(R"({"version":2,"aircraft":"FA-18C_hornet","state":"flying","values":{}})",f,error));
    CHECK(!parseFrame(R"({"version":1,"aircraft":"FA-18C_hornet","state":"flying","values":[]})",f,error));
    CHECK(parseFrame(R"({"version":1,"aircraft":"FA-18C_hornet","state":"flying","values":{"shake":0.4,"missing":"bad","huge":1e20}})",f,error));
    CHECK(f.value("shake")==.4);CHECK(!f.value("missing"));CHECK(!f.value("huge"));
    auto j=Settings{}.json();j["master"]=900;j["motorCap"]=100;j["effects"]["gun"]["motorMin"]=-10;
    auto s=Settings::fromJson(j);CHECK(s.master==1);CHECK(s.motorCap==25);CHECK(s.effects[Gun].motorMin==10);
    j["motorFloor"]=100;j["motorCurve"]=-3;auto calibrated=Settings::fromJson(j);CHECK(calibrated.motorFloor==25);CHECK(calibrated.motorCurve==.25f);
    j.erase("motorFloor");j.erase("motorCurve");CHECK(Settings::fromJson(j).motorCurve==1); // Existing profiles remain compatible.
    j["gearStartMs"]=-10;j["gearLockMs"]=9000;j["gearDemoSeconds"]=100;auto gearLimits=Settings::fromJson(j);CHECK(gearLimits.gearStartMs==50&&gearLimits.gearLockMs==1000&&gearLimits.gearDemoSeconds==15);
    j["gearStartGapMs"]=-1;j["gearLockGapMs"]=9000;j["gearRampMs"]=9000;gearLimits=Settings::fromJson(j);CHECK(gearLimits.gearStartGapMs==0&&gearLimits.gearLockGapMs==1500&&gearLimits.gearRampMs==3000);
    Settings reference;reference.master=.4f;reference.motorCap=18;reference.effects[Gun].gain=.3f;applyGearPreset(reference);
    CHECK(reference.master==.4f&&reference.motorCap==18&&reference.effects[Gun].gain==.3f);auto roundTrip=Settings::fromJson(reference.json());
    CHECK(roundTrip.gearStartGapMs==300&&roundTrip.gearLockGapMs==460&&roundTrip.gearRampMs==1200&&roundTrip.gearDemoSeconds==4.8f);
    // Unrecognized activation flags must not affect output settings.
    for(bool oldGate:{false,true}){
        auto legacy=reference.json();legacy["calibrated"]=oldGate;legacy["liveEnabled"]=oldGate;legacy["muted"]=true;legacy["autoRun"]=oldGate;
        auto migrated=Settings::fromJson(legacy);auto saved=migrated.json();
        CHECK(!saved.contains("calibrated")&&!saved.contains("liveEnabled")&&!saved.contains("autoRun"));
        CHECK(migrated.muted&&migrated.master==reference.master&&migrated.motorCap==reference.motorCap);
        CHECK(saved.at("effects")==legacy.at("effects")&&migrated.gearDemoSeconds==reference.gearDemoSeconds);
    }
    j["effects"]["gun"]["releaseMs"]="broken";bool rejected=false;try{Settings::fromJson(j);}catch(...){rejected=true;}CHECK(rejected);
}
static void sustainedTests(){
    // Upgrading adds the new cue without replacing existing per-effect tuning.
    Settings custom;custom.effects[Gear].motorMax=24;custom.effects[Countermeasures].motorMax=19;
    auto legacy=custom.json();legacy["effects"].erase("afterburner_rumble");
    auto migrated=Settings::fromJson(legacy);auto saved=migrated.json();
    CHECK(migrated.effects[AfterburnerRumble].enabled);
    for(auto it=legacy["effects"].begin();it!=legacy["effects"].end();++it)CHECK(saved["effects"][it.key()]==it.value());
    migrated.effects[AfterburnerRumble].enabled=false;migrated.effects[AfterburnerRumble].motorMax=18;
    auto roundTrip=Settings::fromJson(migrated.json());CHECK(!roundTrip.effects[AfterburnerRumble].enabled&&roundTrip.effects[AfterburnerRumble].motorMax==18);

    Settings s;EffectEngine engine;Frame f;f.state="flying";f.aircraft="FA-18C_hornet";f.session=5;
    int ignitionStarts=0;bool wasIgnition=false;
    for(int n=0;n<1500;++n){
        const auto now=uint64_t(1000+n*20);f.sequence=n+1;f.simTime=n*.02;
        f.values={{"ab_left",n<5?0:.6},{"ab_right",n<10?0:.8},{"cannon_rounds",n<250?500:490}};
        engine.ingest(f,now);auto mix=engine.tick(now,s);
        const bool ignition=mix.effects[Afterburner].reason=="Ignition kick";
        if(ignition&&!wasIgnition)++ignitionStarts;wasIgnition=ignition;
        if((n>100&&n<240) || n>300){CHECK(mix.dominant==AfterburnerRumble);CHECK(mix.motor>=12&&mix.motor<=16);}
        if(n==251)CHECK(mix.dominant==Gun&&mix.motor==25);
        // At the first tick after the 180 ms burst + 60 ms settle, resume the
        // already-running bed. The 300 ms retrigger recovery must not mute it.
        if(n>=262&&n<=300)CHECK(mix.dominant==AfterburnerRumble&&mix.motor>=12);
        if(n>100)CHECK(mix.motor>0);
    }
    CHECK(ignitionStarts==1); // The second engine and steady AB must not keep firing ignition kicks.
    const uint64_t now=31000;
    auto stale=engine;CHECK(stale.tick(now+400,s).motor==0);
    auto disabled=s;disabled.effects[AfterburnerRumble].enabled=false;CHECK(engine.tick(now,disabled).motor==0);
    auto zeroMaster=s;zeroMaster.master=0;CHECK(engine.tick(now,zeroMaster).motor==0);
    // Losing either required engine signal stops immediately; return is a fresh ignition baseline.
    f.sequence++;f.simTime+=.02;f.values.erase("ab_right");engine.ingest(f,now);CHECK(engine.tick(now,s).motor==0);
    f.sequence++;f.simTime+=.02;f.values["ab_right"]=.8;engine.ingest(f,now+20);
    CHECK(engine.tick(now+20,s).effects[Afterburner].level==0);
    // A new session already in AB gets ongoing rumble without an invented onset.
    f.session++;f.sequence=1;f.simTime=0;engine.ingest(f,now+40);
    CHECK(engine.tick(now+40,s).effects[Afterburner].level==0);
    // Leaving AB has a bounded fade, independent of normal engine ambience.
    for(int n=0;n<40;++n){f.sequence++;f.simTime+=.02;f.values["ab_left"]=f.values["ab_right"]=0;
        engine.ingest(f,now+60+n*20);auto mix=engine.tick(now+60+n*20,s);if(n>=16)CHECK(mix.motor==0);}
    // Gear and speedbrake airflow both persist beyond their deployment, until contact or missing data.
    for(const char* deployed:{"gear","airbrake"}){
        engine.reset();auto airflow=s;for(size_t i=0;i<effectCount;++i)airflow.effects[i].enabled=i==Airflow;
        f.values={{"on_ground",0},{"ias_mps",80},{deployed,1}};f.session++;
        for(int n=0;n<500;++n){f.sequence=n+1;f.simTime=n*.02;engine.ingest(f,40000+n*20);auto mix=engine.tick(40000+n*20,airflow);
            if(n>20)CHECK(mix.dominant==Airflow&&mix.motor>=11&&mix.motor<=13);}
        f.values["shake"]=1;f.sequence++;f.simTime+=.02;engine.ingest(f,50000);auto mix=engine.tick(50000,s);
        CHECK(mix.dominant==Airflow&&mix.effects[Buffet].reason=="Background suppressed");
        f.values["on_ground"]=1;
        for(int n=0;n<30;++n){f.sequence++;f.simTime+=.02;engine.ingest(f,50020+n*20);mix=engine.tick(50020+n*20,airflow);if(n>=17)CHECK(mix.motor==0);}
        f.values.erase("ias_mps");f.sequence++;f.simTime+=.02;engine.ingest(f,50620);mix=engine.tick(50620,airflow);
        CHECK(!mix.effects[Airflow].available&&mix.motor==0);
    }
}
static void buffetPriorityTests(){
    Settings s;CHECK(s.effects[Buffet].enabled&&s.effects[Buffet].priority==25);
    auto legacy=s.json();legacy.erase("mixRevision");legacy["effects"]["buffet"]["priority"]=65;
    legacy["effects"]["gear"]["motorMax"]=24;legacy["effects"]["countermeasures"]["motorMax"]=19;
    auto migrated=Settings::fromJson(legacy).json();auto expected=legacy;
    expected["mixRevision"]=1;expected["effects"]["buffet"]["priority"]=25;expected["aircraftTuning"]=migrated["aircraftTuning"];CHECK(migrated==expected);
    legacy["effects"]["buffet"]["priority"]=42;CHECK(Settings::fromJson(legacy).effects[Buffet].priority==42);
    migrated["effects"]["buffet"]["priority"]=65;CHECK(Settings::fromJson(migrated).effects[Buffet].priority==65);
    // Strong continuous buffet must yield to each flight cue, with gear retaining its deliberate internal quiet gaps.
    for(size_t cue:{size_t(Gear),size_t(Gun),size_t(Touchdown),size_t(Afterburner),size_t(AfterburnerRumble),size_t(Stores),size_t(Countermeasures),size_t(Airflow)}){
        auto isolated=s;for(size_t i=0;i<effectCount;++i)isolated.effects[i].enabled=i==Buffet||i==cue;
        EffectEngine engine;bool tookOver=false;
        for(int n=0;n*.02<effectDemoDuration(cue,isolated);++n){
            const double t=n*.02;const auto now=uint64_t(200000+n*20);
            auto f=cue==Gear?gearDemoFrame(t,now,isolated.gearDemoSeconds,gearDemoRestSeconds(isolated)):effectDemoFrame(cue,t,now);
            if(cue!=Touchdown)f.values["on_ground"]=0;f.values["shake"]=1;
            engine.ingest(f,now);auto mix=engine.tick(now,isolated);
            const auto& activity=mix.effects[cue];
            if(activity.level>=.02f){CHECK(mix.dominant==int(cue));tookOver=true;}
            if(activity.reason.find("quiet")!=std::string::npos)CHECK(mix.dominant==int(cue)&&mix.motor==0);
        }
        CHECK(tookOver);
    }
}
static void engineTests(){
    Settings s;for(auto& e:s.effects){e.enabled=true;e.attackMs=0;e.threshold=0;e.gain=1;}s.master=1;
    EffectEngine engine;auto f=frame(1);engine.ingest(f,1000);auto m=engine.tick(1000,s);
    CHECK(m.effects[Gun].level==0);CHECK(m.effects[Touchdown].level==0);CHECK(m.effects[Gear].level==0);
    f.sequence++;f.simTime+=.02;f.values["cannon_rounds"]-=10;engine.ingest(f,1020);m=engine.tick(1020,s);
    CHECK(m.effects[Gun].level>.5);CHECK(m.dominant==Gun);CHECK(m.motor>=10&&m.motor<=s.motorCap);
    CHECK(engine.tick(1400,s).motor==0); // Paused simulator cannot sustain an effect.
    engine.reset();f=frame(2);engine.ingest(f,2000);f.sequence++;f.simTime+=.02;f.values["cannon_rounds"]=999;
    engine.ingest(f,2020);CHECK(engine.tick(2020,s).effects[Gun].level==0); // Rearm is not gunfire.
    f.sequence++;f.session++;f.simTime=0;f.values["cannon_rounds"]=100;
    engine.ingest(f,2040);CHECK(engine.tick(2040,s).effects[Gun].level==0); // New mission is not a drop event.
    engine.reset();f=frame(3);f.values["vertical_mps"]=-4;engine.ingest(f,2500);
    for(int i=1;i<=25;++i){f.sequence++;f.simTime+=.02;engine.ingest(f,2500+i*20);engine.tick(2500+i*20,s);}
    f.simTime+=.02;f.sequence++;f.values["on_ground"]=1;engine.ingest(f,3020);m=engine.tick(3020,s);CHECK(m.dominant==Touchdown);
    f.aircraft="F-16C_50";engine.ingest(f,3040);CHECK(engine.tick(3040,s).effects[Touchdown].level==0);
    engine.reset();f=frame(3.5);engine.ingest(f,3500);f.simTime+=1;f.sequence++;f.values["cannon_rounds"]-=30;engine.ingest(f,3900);
    CHECK(engine.tick(3900,s).effects[Gun].level==0); // Reconnect cannot infer a burst from an old ammunition baseline.
    engine.reset();f=frame(4);f.values["ab_left"]=0;f.values["ab_right"]=0;engine.ingest(f,4000);
    for(int i=1;i<=4;++i){f.sequence++;f.simTime+=.02;f.values["ab_left"]=i*.05;engine.ingest(f,4000+i*20);}
    CHECK(engine.tick(4080,s).effects[Afterburner].level>.5); // A gradual threshold crossing still ignites.
    engine.reset();engine.trigger(Gun,5000);CHECK(engine.tick(5000,s,true).motor>0);CHECK(engine.tick(6500,s,true).motor==0);
    CHECK(mapMotor(0,10,25,25)==0);CHECK(mapMotor(std::numeric_limits<float>::quiet_NaN(),10,25,25)==0);
    for(int n=1;n<=100;++n){auto mapped=mapMotor(n/100.f,10,25,18);CHECK(mapped==0||(mapped>=10&&mapped<=18));}
    CHECK(mapMotor(.5f,10,25,25,.5f)>mapMotor(.5f,10,25,25,1));
    for(float curve:{.25f,1.f,3.f}){int previous=0;for(int n=0;n<=100;++n){int mapped=mapMotor(n/100.f,10,25,18,curve);CHECK(mapped>=previous&&mapped<=18);previous=mapped;}CHECK(previous==18);}
    CHECK(mapMotor(0,20,25,25,.25f)==0);CHECK(mapMotor(1,20,25,25,.25f)==25);
    Settings fitted;fitEffectRanges(fitted,14,21);CHECK(fitted.motorFloor==14&&fitted.motorCap==21);
    for(const auto& e:fitted.effects)CHECK(e.motorMin>=14&&e.motorMin<=e.motorMax&&e.motorMax<=21);
    engine.reset();f=gearDemoFrame(0,1000);f.values["damage_total"]=3;engine.ingest(f,1000);CHECK(engine.tick(1000,s).effects[Damage].level==0);
    f.sequence++;f.simTime=.02;f.values["damage_total"]=4;engine.ingest(f,1020);CHECK(engine.tick(1020,s).effects[Damage].level==0);
    engine.reset();f=gearDemoFrame(0,2000);f.values["damage_total"]=4;engine.ingest(f,2000);f.sequence++;f.simTime=.02;f.values["damage_total"]=3;engine.ingest(f,2020);CHECK(engine.tick(2020,s).effects[Damage].level==0);
    engine.reset();f=gearDemoFrame(0,3000);f.values["on_ground"]=0;f.values["ias_mps"]=140;engine.ingest(f,3000);CHECK(engine.tick(3000,s).dominant==Airflow);
    f.sequence++;f.simTime=.02;f.values["on_ground"]=1;engine.ingest(f,3020);CHECK(engine.tick(4000,s).motor==0);
}
static void cueTests(){
    Settings defaults;CHECK(defaults.effects[Airflow].enabled&&defaults.effects[Stores].enabled&&defaults.effects[Countermeasures].enabled);
    CHECK(!defaults.effects[Engine].enabled&&!defaults.effects[Taxi].enabled&&effectSupported(Engine)&&effectSupported(Countermeasures)&&!effectSupported(Damage));
    auto legacy=defaults.json();legacy["effects"]["damage"]["enabled"]=true;CHECK(!Settings::fromJson(legacy).effects[Damage].enabled);
    auto kept=defaults;kept.effects[Gear].motorMax=22;kept.gearStartMs=270;kept.gearDemoSeconds=12.1f;kept.master=.7f;kept.motorCap=21;kept.motorCurve=.8f;applyHeadsetMix(kept);
    CHECK(kept.effects[Gear].motorMax==22&&kept.gearStartMs==270&&kept.gearDemoSeconds==12.1f&&kept.master==.7f&&kept.motorCap==21&&kept.motorCurve==.8f);
    for(size_t cue=0;cue<effectCount;++cue){
        if(!effectSupported(cue)||cue==Gear)continue;
        auto s=defaults;for(size_t i=0;i<effectCount;++i)s.effects[i].enabled=i==cue;
        EffectEngine engine;int peak=0;auto duration=effectDemoDuration(cue,s);
        for(int step=0;step*.02<duration+.1;++step){const double t=step*.02;const auto now=uint64_t(10000+step*20);engine.ingest(effectDemoFrame(cue,t,now),now);auto m=engine.tick(now,s);peak=std::max(peak,m.motor);
            if(t<.38)CHECK(m.motor==0);
            if(cue==Gun&&((t>.7&&t<1.3)||(t>2.4&&t<2.7)))CHECK(m.motor>=20);
            if(cue==Gun&&t>1.7&&t<2.28)CHECK(m.motor==0);
            if((cue==Stores||cue==Countermeasures)&&t>.62)CHECK(m.motor==0);
            if(cue==Afterburner&&t>.44&&t<.56)CHECK(m.motor==21);
            if(cue==Afterburner&&t>.8&&t<1.05)CHECK(m.motor>=13&&m.motor<=16);
            if(cue==Afterburner&&t>1.32)CHECK(m.motor==0);
            if(cue==Touchdown&&t>1.2)CHECK(m.motor==0);
            CHECK(m.motor>=0&&m.motor<=s.motorCap);
        }
        CHECK(peak>0);CHECK(engine.tick(20000,s).motor==0);
    }
    // Fast dispense/release events cannot extend a sharp pulse or queue output.
    for(size_t cue:{Stores,Countermeasures}){auto s=defaults;EffectEngine engine;
        for(int n=0;n<20;++n){auto now=uint64_t(30000+n*20);engine.trigger(cue,now,1);auto m=engine.tick(now,s,true);if(n>8)CHECK(m.motor==0);}
        CHECK(engine.tick(31000,s,true).motor==0);
    }
    // Missing ammo stops an active burst and cannot resume an old one on return.
    {auto s=defaults;EffectEngine engine;auto f=effectDemoFrame(Gun,.5,40000);engine.ingest(f,40000);f.sequence++;f.simTime+=.02;f.values["cannon_rounds"]-=4;engine.ingest(f,40020);CHECK(engine.tick(40020,s).motor>0);
        f.sequence++;f.simTime+=.02;f.values.clear();engine.ingest(f,40040);CHECK(engine.tick(40040,s).motor==0);
        f.sequence++;f.simTime+=.02;f.values["cannon_rounds"]=450;engine.ingest(f,40060);CHECK(engine.tick(40060,s).motor==0);}
    // Continuous cues have a bounded stop even with an exaggerated release filter.
    {auto s=defaults;s.effects[Buffet].releaseMs=1000;s.effects[Buffet].settleMs=120;EffectEngine engine;
        for(int n=0;n<100;++n){auto now=uint64_t(42000+n*20);auto f=effectDemoFrame(Buffet,n*.02,now);f.values["shake"]=n<70?.8:0;engine.ingest(f,now);auto m=engine.tick(now,s);if(n>=77)CHECK(m.motor==0);}}
    // Steady rolling acceleration is not a perpetual runway buzz.
    {auto s=defaults;s.effects[Taxi].enabled=true;EffectEngine engine;for(int n=0;n<50;++n){auto now=uint64_t(45000+n*20);auto f=effectDemoFrame(Taxi,n*.02,now);f.values["accel_y_g"]=1;engine.ingest(f,now);CHECK(engine.tick(now,s).motor==0);}}
    // A short airborne contact bounce cannot create a touchdown event.
    {EffectEngine engine;auto f=effectDemoFrame(Touchdown,0,47000);engine.ingest(f,47000);f.sequence++;f.simTime=.1;f.values["on_ground"]=1;engine.ingest(f,47100);CHECK(engine.tick(47100,defaults).motor==0);}
}
static void gearTests(){
    Settings s;EffectEngine engine;
    for(double stroke:{3.,4.8,8.,15.}){
        engine.reset();int startPeaks[2]{},lockPeaks[2]{},travelMin=25,travelMax=0;int quietChecks=0;
        const double rest=gearDemoRestSeconds(s),duration=gearDemoDuration(stroke,rest);
        for(int i=0;i*.02<=duration;++i){double t=i*.02;auto now=1000+i*20;engine.ingest(gearDemoFrame(t,now,stroke,rest),now);auto m=engine.tick(now,s);
            CHECK(m.effects[Airflow].level==0); // Audition must never include gear drag.
            if(t<.4 || t>duration-.25)CHECK(m.motor==0);
            for(int direction=0;direction<2;++direction){
                double start=.4+direction*(stroke+rest),end=start+stroke;
                if(t>start+.06&&t<start+.18)startPeaks[direction]=std::max(startPeaks[direction],m.motor);
                if((t>start+.26&&t<start+.48)||(t>end+.06&&t<end+.40)){CHECK(m.motor==0);++quietChecks;}
                if(t>end+.51&&t<end+.68)lockPeaks[direction]=std::max(lockPeaks[direction],m.motor);
                if(t>start+2&&t<end-.1){travelMin=std::min(travelMin,m.motor);travelMax=std::max(travelMax,m.motor);}
            }
            if(t==1.5){auto paused=engine;CHECK(paused.tick(now+400,s).motor==0);}
        }
        CHECK(quietChecks>40);CHECK(startPeaks[0]==20&&startPeaks[1]==20);CHECK(lockPeaks[0]==19&&lockPeaks[1]==19);CHECK(travelMin==14&&travelMax==16);
    }
    engine.reset();s.master=.3f;s.effects[Gear].gain=.5f;s.effects[Gear].threshold=.05f;
    for(int i=0;i<300;++i){auto now=21000+i*20;double t=i*.02;engine.ingest(gearDemoFrame(t,now),now);auto m=engine.tick(now,s);if(t>1&&t<6)CHECK(m.motor>0);}
    s.master=0;CHECK(engine.tick(27000,s).motor==0);s.master=1;s.effects[Gear].gain=0;CHECK(engine.tick(27000,s).motor==0);
    s=Settings{};engine.reset();auto f=gearDemoFrame(1.5,8000);engine.ingest(f,8000);CHECK(engine.tick(8000,s).motor==0); // Joining mid-transit cannot invent a start stab.
    // Deliberate gaps remain zero despite long software release and live airflow.
    engine.reset();s.effects[Gear].releaseMs=1000;
    for(int i=0;i<50;++i){auto now=30000+i*20;double t=i*.02;f=gearDemoFrame(t,now);f.values["on_ground"]=0;f.values["ias_mps"]=140;engine.ingest(f,now);auto m=engine.tick(now,s);
        if(t>.68&&t<.86){CHECK(m.effects[Airflow].level>0);CHECK(m.motor==0&&m.dominant==Gear);}
    }
    // An urgent cue can interrupt a quiet gap; the gap is not a global mute.
    engine.reset();for(int i=0;i<=40;++i){auto now=32000+i*20;f=gearDemoFrame(i*.02,now);f.values["cannon_rounds"]=500;engine.ingest(f,now);if(i==38)engine.trigger(Gun,now,1);auto m=engine.tick(now,s);if(i>=38)CHECK(m.dominant==Gun&&m.motor>=20);}
    // Pausing during the final coast must not produce a delayed lock hit.
    s=Settings{};engine.reset();for(int i=0;i<=260;++i){auto now=34000+i*20;engine.ingest(gearDemoFrame(i*.02,now,4.8),now);engine.tick(now,s);}
    auto paused=engine;CHECK(paused.tick(39700,s).motor==0);
    // Stop/reversal during the coast cancels the old endpoint gesture.
    f=gearDemoFrame(5.4,39400,4.8);f.sequence=300;f.values["gear"]=.05;engine.ingest(f,39400);CHECK(engine.tick(39400,s).motor==20);
}
#ifndef SHAKER_CORE_ONLY
static void integrationTests(){
    const std::string original="-- Existing exports\r\ndofile('Tacview.lua')\r\nrequire('telemffb')\r\n";
    const std::string hook="-- BEGIN dcs2psvr2-shaker\nour_hook()\n-- END dcs2psvr2-shaker\n";
    CHECK(withoutHook(original+hook)==original);CHECK(withoutHook(original+hook+hook)==original);
    bool rejected=false;try{withoutHook(original+"-- BEGIN dcs2psvr2-shaker\n");}catch(...){rejected=true;}CHECK(rejected);
    auto root=fs::temp_directory_path()/(L"dcs2psvr2-shaker-test-"+std::to_wstring(GetCurrentProcessId()));
    CHECK(!fs::exists(root));fs::create_directories(root/L"profile/Config");fs::create_directories(root/L"profile/Scripts");fs::create_directories(root/L"source/dcs");
    writeTextAtomic(root/L"source/dcs/Export.lua","-- test export");writeTextAtomic(root/L"source/dcs2psvr2-shaker-bridge.dll","test bridge");
    writeTextAtomic(root/L"profile/Scripts/Export.lua",original);installIntegration(root/L"profile",root/L"source");
    CHECK(integrationInstalled(root/L"profile"));auto once=readText(root/L"profile/Scripts/Export.lua");installIntegration(root/L"profile",root/L"source");CHECK(readText(root/L"profile/Scripts/Export.lua")==once);
    fs::remove(root/L"profile/Scripts/dcs2psvr2-shaker/Export.lua");CHECK(!integrationInstalled(root/L"profile"));installIntegration(root/L"profile",root/L"source");
    fs::remove(root/L"profile/Scripts/dcs2psvr2-shaker/dcs2psvr2-shaker-bridge.dll");CHECK(!integrationInstalled(root/L"profile"));installIntegration(root/L"profile",root/L"source");
    writeTextAtomic(root/L"profile/Scripts/Export.lua",original+hook);CHECK(!integrationInstalled(root/L"profile"));
    writeTextAtomic(root/L"profile/Scripts/Export.lua",once.substr(0,once.find("-- END dcs2psvr2-shaker")));CHECK(!integrationInstalled(root/L"profile"));
    writeTextAtomic(root/L"profile/Scripts/Export.lua",once);CHECK(integrationInstalled(root/L"profile"));
    CHECK(readText(root/L"profile/Scripts/Export.lua.before-dcs2psvr2-shaker")==original);removeIntegration(root/L"profile");CHECK(readText(root/L"profile/Scripts/Export.lua")==original);
    for(const auto& name:{L"profile/Scripts/Export.lua",L"profile/Scripts/Export.lua.before-dcs2psvr2-shaker",L"source/dcs/Export.lua",L"source/dcs2psvr2-shaker-bridge.dll"})fs::remove(root/name);
    for(const auto& name:{L"profile/Scripts",L"profile/Config",L"profile",L"source/dcs",L"source"})fs::remove(root/name);fs::remove(root);
}
#endif
int main(){try{parseTests();flightFeedTests();engineTests();gearTests();cueTests();flightDemoTests();sustainedTests();buffetPriorityTests();
#ifndef SHAKER_CORE_ONLY
integrationTests();
#endif
std::cout<<"PASS: profile migration, real-feed status, demo timeline, event transitions, sustained afterburner/airflow, gear rhythm, effect envelopes, stop/recovery, buffet priority (Windows builds also check export integration)\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
