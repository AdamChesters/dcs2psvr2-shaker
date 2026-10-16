#include "core.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(#x)+" line "+std::to_string(__LINE__));}while(0)
static Frame frame(const std::string& name,uint64_t now){
    Frame f;f.aircraft=name;f.state="flying";f.session=1;f.sequence=now;f.simTime=now/1000.;
    return f;
}
static void profiles(){
    const char* names[]={"FA-18C_hornet","F-16C_50","A-10C","A-10C_2","F-14B","F-14A-135-GR","F-14A-95-GR","F-4E-45MC","AH-64D_BLK_II"};
    for(auto name:names){
        CHECK(aircraftProfile(name));FlightFeed feed;
        auto f=frame(name,1000);f.values["cannon_rounds"]=100;feed.observe(f,1000);
        f=frame(name,1020);f.values["cannon_rounds"]=99;feed.observe(f,1020);CHECK(feed.status(1020,300).supported);
        EffectEngine e;f=frame(name,1000);f.values["cannon_rounds"]=100;e.ingest(f,1000);
        f=frame(name,1020);f.values["cannon_rounds"]=99;e.ingest(f,1020);CHECK(e.tick(1020,Settings{}).motor>0);
        CHECK(e.tick(1400,Settings{}).motor==0);
        f=frame("unknown",1420);f.values["cannon_rounds"]=98;e.ingest(f,1420);CHECK(e.tick(1420,Settings{}).motor==0);
    }
    for(auto name:{"A-10A","F-15C","P-51D","FA-18-fake","F-4E-AI","AH-64A"})CHECK(!aircraftProfile(name));
    Settings s;s.profile="My Hornet";s.effects[Gun].gain=.31f;s.motorCap=18;s.muted=true;s.toolkitPath="local";
    CHECK(s.selectAircraft("viper"));CHECK(s.activeAircraft=="viper");CHECK(s.muted&&s.motorCap==18&&s.toolkitPath=="local");
    s.effects[Gun].gain=.72f;s.profile="My Viper";auto saved=s.json();s=Settings::fromJson(saved);
    CHECK(s.selectAircraft("hornet"));CHECK(s.profile=="My Hornet"&&s.effects[Gun].gain==.31f);
    CHECK(s.selectAircraft("viper"));CHECK(s.profile=="My Viper"&&s.effects[Gun].gain==.72f);
    CHECK(!s.selectAircraft("invalid"));
    CHECK(s.selectAircraft("apache"));CHECK(!s.effects[Gear].enabled&&!s.effects[Airflow].enabled&&!s.effects[Afterburner].enabled&&!s.effects[Catapult].enabled);
    auto old=Settings{}.json();old.erase("activeAircraft");old.erase("aircraftTuning");old["profile"]="Legacy custom";
    auto migrated=Settings::fromJson(old);CHECK(migrated.activeAircraft=="hornet");CHECK(migrated.selectAircraft("tomcat"));CHECK(migrated.selectAircraft("hornet"));CHECK(migrated.profile=="Legacy custom");
}
static void capabilities(){
    for(auto name:{"F-16C_50","A-10C_2","F-14B","F-4E-45MC","AH-64D_BLK_II"}){
        EffectEngine e;auto f=frame(name,1000);
        f.values={{"ab_left",0},{"ab_right",0},{"gear",0},{"on_ground",0},{"ias_mps",140},{"rpm_left_pct",90}};
        e.ingest(f,1000);f.sequence=1020;f.simTime=1.02;f.values["ab_left"]=1;f.values["gear"]=.2;
        if(std::string(name)=="F-16C_50")f.values.erase("ab_right");
        e.ingest(f,1020);auto m=e.tick(1020,Settings{});auto* p=aircraftProfile(name);
        CHECK(m.effects[Afterburner].available==p->afterburner);
        CHECK(m.effects[Gear].available==!p->helicopter);
        CHECK(m.effects[Airflow].available==!p->helicopter);
        CHECK(m.effects[Engine].available==(p->engines==1));
        CHECK(!m.effects[Catapult].available);
        f.sequence=1040;f.simTime=1.04;f.values.clear();e.ingest(f,1040);CHECK(e.tick(1040,Settings{}).motor==0);
    }
    // A twin's missing AB signal must not invent a single-engine fallback.
    EffectEngine e;auto f=frame("F-14B",1000);f.values={{"ab_left",1}};e.ingest(f,1000);CHECK(!e.tick(1000,Settings{}).effects[AfterburnerRumble].available);
}
static Mix touchdown(double fpm,const char* aircraft="FA-18C_hornet"){
    Settings s;for(size_t i=0;i<effectCount;++i)s.effects[i].enabled=i==Touchdown;
    EffectEngine e;
    for(uint64_t now=1000;now<=1600;now+=20){auto f=frame(aircraft,now);f.values={{"on_ground",0},{"vertical_mps",-fpm*.00508}};e.ingest(f,now);e.tick(now,s);}
    auto f=frame(aircraft,1620);f.values={{"on_ground",1},{"vertical_mps",0}};e.ingest(f,1620);return e.tick(1620,s);
}
static void landing(){
    for(auto aircraft:{"FA-18C_hornet","F-16C_50","A-10C_2","F-14B","F-4E-45MC","AH-64D_BLK_II"}){
        CHECK(touchdown(0,aircraft).motor==0);CHECK(touchdown(30,aircraft).motor==0);CHECK(touchdown(-100,aircraft).motor==0);
        auto half=touchdown(250,aircraft), full=touchdown(500,aircraft), hard=touchdown(1000,aircraft);
        CHECK(std::abs(half.effects[Touchdown].level-.5f)<.001f);
        CHECK(std::abs(full.effects[Touchdown].level-1.f)<.001f);
        CHECK(half.motor<full.motor);CHECK(full.motor==25);CHECK(hard.motor==full.motor);
    }
}
static void catapult(){
    for(auto name:{"FA-18C_hornet","F-14B","F-4E-45MC","F-16C_50","AH-64D_BLK_II"}){
        for(int scenario=0;scenario<5;++scenario){
            Settings s;for(size_t i=0;i<effectCount;++i)s.effects[i].enabled=i==Catapult;
            EffectEngine e;int peak=0;
            for(int n=0;n<220;++n){
                auto now=uint64_t(1000+n*20);auto f=effectDemoFrame(Catapult,n*.02,now);f.aircraft=name;
                if(scenario==1)f.values["launch_bar"]=0; // Normal runway takeoff.
                if(scenario==2)f.values["accel_x_g"]=.4; // Taxi/ordinary acceleration.
                if(scenario==3)f.values.erase("launch_bar");
                if(scenario==4 && n<50)continue; // Joining during the shot lacks an observed baseline.
                e.ingest(f,now);auto m=e.tick(now,s);peak=std::max(peak,m.motor);
                if(n>160)CHECK(m.motor==0);
            }
            if(scenario==0 && aircraftProfile(name)->carrier)CHECK(peak==25);else CHECK(peak==0);
            CHECK(e.tick(7000,s).motor==0);
        }
    }
    // A long acceleration is bounded; a stale gap, lost signal or aircraft change cancels it.
    for(int scenario=0;scenario<4;++scenario){
        Settings s;for(size_t i=0;i<effectCount;++i)s.effects[i].enabled=i==Catapult;
        EffectEngine e;bool fired=false;
        for(int n=0;n<350;++n){
            auto now=uint64_t(1000+n*20);auto f=frame("FA-18C_hornet",now);
            f.values={{"on_ground",1},{"launch_bar",1},{"ground_mps",n<40?10:10+(n-40)*.5},{"accel_x_g",n<40?0:2.5}};
            if(n==70 && scenario==1)CHECK(e.tick(now+400,s).motor==0);
            if(scenario==1 && n>=70){now+=500;f.sequence=now;f.simTime=now/1000.;}
            if(scenario==2 && n>=70)f.values.erase("launch_bar");
            if(scenario==3 && n>=70)f.aircraft="F-4E-45MC";
            e.ingest(f,now);auto m=e.tick(now,s);if(m.motor>0)fired=true;
            if(n>270 || (scenario && n>90))CHECK(m.motor==0);
        }
        CHECK(fired);
    }
}
static void effectLists(){
    for(const auto& p:aircraftProfiles()){
        size_t count=0;
        for(size_t i=0;i<effectCount;++i)count+=effectSupported(i,&p);
        CHECK(count==(p.helicopter?7:p.carrier?12:p.afterburner?11:9));
        CHECK(effectSupported(Gun,&p)&&effectSupported(Touchdown,&p)&&effectSupported(Taxi,&p));
        CHECK(effectSupported(Afterburner,&p)==p.afterburner);
        CHECK(effectSupported(AfterburnerRumble,&p)==p.afterburner);
        CHECK(effectSupported(Catapult,&p)==p.carrier);
        CHECK(effectSupported(Gear,&p)==!p.helicopter);
        CHECK(effectSupported(Airflow,&p)==!p.helicopter);
        CHECK(!effectSupported(Damage,&p));
        CHECK(!effectSupported(effectCount,&p));
    }
    CHECK(!effectSupported(Gun,nullptr));
}
static void activity(){
    Settings s;s.effects[Engine].enabled=true;
    EffectEngine e;Mix m;
    for(uint64_t now=1000;now<=1600;now+=20){
        auto f=frame("FA-18C_hornet",now);
        f.values={{"on_ground",0},{"gear",0},{"airbrake",.8},{"ias_mps",140},
            {"shake",.8},{"ab_left",1},{"ab_right",1},{"rpm_left_pct",90},{"rpm_right_pct",90},{"cannon_rounds",100}};
        e.ingest(f,now);m=e.tick(now,s);
    }
    CHECK(m.dominant==int(Airflow));
    CHECK(effectActivity(m,Airflow,true)==EffectActivity::Output);
    for(auto i:{Buffet,Engine,AfterburnerRumble})CHECK(effectActivity(m,i,true)==EffectActivity::Active);
    e.trigger(Gun,1620,1);m=e.tick(1620,s);
    CHECK(m.dominant==int(Gun));
    CHECK(effectActivity(m,Gun,true)==EffectActivity::Output);
    for(auto i:{Buffet,Engine,AfterburnerRumble,Airflow})CHECK(effectActivity(m,i,true)==EffectActivity::Active);
    // Muted/faulted/direct-test routing must never claim these cues drive output.
    for(size_t i=0;i<effectCount;++i){auto a=effectActivity(m,i,false);CHECK(a!=EffectActivity::Output&&a!=EffectActivity::Gap);}
    m=e.tick(2500,s);for(size_t i=0;i<effectCount;++i)CHECK(effectActivity(m,i,true)==EffectActivity::Idle);
    // Gear can own the mix while intentionally commanding silence.
    m={};m.dominant=int(Gear);m.effects[Gear].available=true;
    CHECK(effectActivity(m,Gear,true)==EffectActivity::Gap);
    CHECK(effectActivity(m,Gear,false)==EffectActivity::Idle);
    CHECK(effectActivity(m,effectCount,true)==EffectActivity::Idle);
}
int main(){try{profiles();capabilities();landing();catapult();effectLists();activity();std::cout<<"PASS: aircraft profiles, saved tuning, capability guards, touchdown scaling, catapult detection, aircraft effect lists and layered activity\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
