#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace shaker {
const std::array<AircraftProfile,6>& aircraftProfiles(){
    static const std::array<AircraftProfile,6> profiles={{
        {"hornet","F/A-18C Hornet",2,true,false,true},
        {"viper","F-16C Viper",1,true,false,false},
        {"warthog","A-10C / A-10C II",2,false,false,false},
        {"tomcat","F-14A/B Tomcat",2,true,false,true},
        {"phantom","F-4E Phantom",2,true,false,false},
        {"apache","AH-64D Apache",2,false,true,false}
    }};
    return profiles;
}
const AircraftProfile* profileById(const std::string& id){
    for(const auto& p:aircraftProfiles())if(id==p.id)return &p;
    return nullptr;
}
const AircraftProfile* aircraftProfile(const std::string& aircraft){
    const char* id=nullptr;
    if(aircraft=="FA-18C_hornet")id="hornet";
    else if(aircraft=="F-16C_50")id="viper";
    else if(aircraft=="A-10C" || aircraft=="A-10C_2")id="warthog";
    else if(aircraft=="F-14B" || aircraft=="F-14A-135-GR" || aircraft=="F-14A-95-GR")id="tomcat";
    else if(aircraft=="F-4E-45MC")id="phantom";
    else if(aircraft=="AH-64D_BLK_II")id="apache";
    return id?profileById(id):nullptr;
}
static Json tuning(const Json& j){
    Json result=Json::object();
    for(const auto* key:{"profile","mixRevision","master","effects","gearStartMs","gearLockMs","gearStartGapMs","gearLockGapMs","gearRampMs","gearDemoSeconds"})
        if(j.contains(key))result[key]=j[key];
    return result;
}
const EffectDefinition& effectDefinition(size_t i){
    static const std::array<EffectDefinition,effectCount> definitions={{
        {CueShape::Continuous,true,"Airframe feedback only while DCS reports airborne shake. Not an always-on ambience or a dedicated stall/high-alpha warning; other shake sources depend on the aircraft module. Suppressed on the ground and behind higher-priority impacts."},
        {CueShape::Burst,true,"One sustained rumble while cannon ammunition falls, followed by a short settle, then any active background resumes. Individual rounds are too fast for separate headset pulses. Unlimited-ammunition missions may not provide this signal."},
        {CueShape::Impact,true,"A landing thud followed by a brief settle, then any active background resumes. Requires established flight and a descending contact; wheel-contact chatter cannot repeatedly trigger it. Strength scales linearly with descent rate to full at 500 ft/min; below 60 ft/min stays quiet. Carrier deck-relative speed is unavailable."},
        {CueShape::Continuous,false,"Bumps from changing vertical acceleration while rolling. Flat, steady ground stays quiet. Off by default; enable and tune to taste."},
        {CueShape::Mechanism,true,"THUNK → quiet → rising travel rumble → coast → THUNK. Actual gear motion drives live duration. Animation endpoints indicate completion; they are not separately verified mechanical lock sensors."},
        {CueShape::Surge,true,"A firm ignition kick followed by a softer rumble, then any active background resumes. Marks afterburner engagement once. Hysteresis and a cooldown prevent throttle chatter; the two engines are treated as one headset cue."},
        {CueShape::Impact,true,"A quick, sharp release pulse with a retrigger delay. Falling airborne store count includes release AND jettison; it cannot identify weapon type or prove a successful launch. Rapid salvos coalesce instead of building a pulse queue."},
        {CueShape::Continuous,false,"Optional subtle RPM ambience for people without seat or stick haptics. Off by default. Smooth intensity follows RPM; the headset cannot reproduce individual engine harmonics."},
        {CueShape::Impact,true,"A quick pulse when flare or chaff count falls. A short retrigger delay separates cues; fast dispense programs coalesce rather than commanding a pulse for every cartridge. Rearm and missing counts do not trigger output."},
        {CueShape::Continuous,true,"Subtle airflow rumble from airborne gear/speedbrake deployment and airspeed. It approximates configuration drag, not measured turbulence. Gear-mechanism quiet gaps take priority."},
        {CueShape::Unavailable,false,"Deferred: damage animation changes are not reliable individual hit events. Damage and ejection need validated own-aircraft event detection before headset output is enabled."},
        {CueShape::Continuous,true,"A sustained low rumble while either engine is in afterburner. Strength follows the stronger engine's afterburner signal. Tune it separately from the ignition kick and normal engine ambience; higher-priority cues can take over."},
        {CueShape::Continuous,true,"Strong Hornet/Tomcat launch rumble. Inferred from an observed deployed launch bar followed by sustained high forward acceleration on the ground. Not a direct DCS catapult event; runway and carrier validation is required. Stops at liftoff, loss of signals or four seconds."}
    }};
    return definitions.at(i);
}
bool effectSupported(size_t i){return i<effectCount && effectDefinition(i).shape!=CueShape::Unavailable;}
bool effectSupported(size_t i,const AircraftProfile* aircraft){
    if(!aircraft || !effectSupported(i))return false;
    if(i==Afterburner || i==AfterburnerRumble)return aircraft->afterburner;
    if(i==Gear || i==Airflow)return !aircraft->helicopter;
    if(i==Catapult)return aircraft->carrier;
    return true;
}
EffectActivity effectActivity(const Mix& mix,size_t i,bool routed){
    if(i>=effectCount || !mix.effects[i].available)return EffectActivity::Idle;
    if(routed && mix.dominant==int(i))return mix.motor>0?EffectActivity::Output:EffectActivity::Gap;
    return mix.effects[i].level>=.02f?EffectActivity::Active:EffectActivity::Idle;
}
std::optional<double> Frame::value(const char* k) const {
    const auto i = values.find(k); return i == values.end() ? std::nullopt : std::optional(i->second);
}
bool parseFrame(const std::string& payload, Frame& result, std::string& error) {
    try {
        if (payload.empty() || payload.size() > 32768) throw std::runtime_error("Telemetry length invalid");
        auto j = Json::parse(payload);
        if (!j.is_object() || j.value("version",0) != 1) throw std::runtime_error("Unsupported telemetry version");
        Frame f; f.aircraft = j.at("aircraft").get<std::string>(); f.state = j.at("state").get<std::string>();
        if (f.aircraft.size()>128 || (f.state!="flying" && f.state!="stopped" && f.state!="no_aircraft")) throw std::runtime_error("Invalid flight identity");
        const auto& values = j.at("values");
        if (!values.is_object() || values.size()>128) throw std::runtime_error("Invalid telemetry fields");
        for (auto it=values.begin();it!=values.end();++it) {
            if (it.key().size()>64 || !it.value().is_number()) continue;
            const double n = it.value().get<double>();
            if (!std::isfinite(n) || std::abs(n)>1e8) continue;
            f.values[it.key()] = n;
        }
        result = std::move(f); error.clear(); return true;
    } catch (const std::exception& e) { error=e.what(); return false; }
}
void FlightFeed::reset(){current_.reset();advancedAt_=0;}
void FlightFeed::observe(const Frame& f,uint64_t publishedMs){
    if(current_ && f.session==current_->session && f.sequence<=current_->sequence)return;
    if(!current_ || f.session!=current_->session || f.simTime<current_->simTime)advancedAt_=0;
    else if(f.simTime>current_->simTime)advancedAt_=publishedMs;
    current_=f;
}
FlightFeedStatus FlightFeed::status(uint64_t now,int staleMs)const{
    FlightFeedStatus result;
    if(!current_)return result;
    result.aircraft=current_->aircraft;result.ageMs=advancedAt_ && now>=advancedAt_?now-advancedAt_:0;
    result.telemetryLive=advancedAt_ && now>=advancedAt_ && result.ageMs<uint64_t(staleMs) && current_->state!="stopped";
    result.aircraftLive=result.telemetryLive && current_->state=="flying" && !current_->aircraft.empty() && !current_->values.empty();
    result.supported=result.aircraftLive && aircraftProfile(current_->aircraft);
    return result;
}
Settings::Settings() {
    effects[Gun].priority=90; effects[Gun].releaseMs=100; effects[Gun].gain=1;
    effects[Touchdown].priority=100; effects[Touchdown].releaseMs=250;
    effects[Buffet].priority=25; effects[Buffet].threshold=.025f;
    effects[Taxi].priority=15; effects[Taxi].gain=.25f; effects[Taxi].threshold=.02f;
    effects[Gear].priority=45; effects[Afterburner].priority=60;
    effects[Stores].priority=80; effects[Stores].enabled=false;
    effects[Engine].priority=5; effects[Engine].enabled=false; effects[Engine].gain=.2f;
    effects[Countermeasures].priority=50; effects[Countermeasures].enabled=false;
    effects[Buffet].motorMin=12; effects[Buffet].motorMax=18; effects[Buffet].attackMs=70; effects[Buffet].releaseMs=220; effects[Buffet].gain=1;
    effects[Gun].motorMin=20; effects[Gun].motorMax=25;
    effects[Touchdown].motorMin=10; effects[Touchdown].motorMax=25; effects[Touchdown].gain=1;
    effects[Taxi].motorMax=11; effects[Taxi].attackMs=100; effects[Taxi].releaseMs=250;
    effects[Gear].motorMin=14; effects[Gear].motorMax=20; effects[Gear].attackMs=0; effects[Gear].releaseMs=40; effects[Gear].gain=1; effects[Gear].threshold=0;
    effects[Afterburner].motorMin=15; effects[Afterburner].motorMax=20;
    effects[Stores].motorMin=15; effects[Stores].motorMax=20; effects[Stores].gain=1;
    effects[Engine].motorMax=10; effects[Engine].attackMs=200; effects[Engine].releaseMs=300;
    effects[Countermeasures].motorMin=12; effects[Countermeasures].motorMax=15;
    effects[Airflow].motorMin=12; effects[Airflow].motorMax=15; effects[Airflow].gain=1; effects[Airflow].priority=35; effects[Airflow].attackMs=100; effects[Airflow].releaseMs=220;
    effects[Damage].motorMin=20; effects[Damage].motorMax=25; effects[Damage].gain=1; effects[Damage].priority=100; effects[Damage].enabled=false;
    effects[Gun].attackMs=0;effects[Gun].releaseMs=40;effects[Gun].threshold=0;effects[Gun].holdMs=180;effects[Gun].settleMs=60;effects[Gun].coastMs=300;effects[Gun].cooldownMs=0;
    effects[Touchdown].attackMs=0;effects[Touchdown].releaseMs=30;effects[Touchdown].threshold=0;effects[Touchdown].holdMs=220;effects[Touchdown].settleMs=140;effects[Touchdown].coastMs=350;effects[Touchdown].cooldownMs=1100;
    effects[Buffet].attackMs=80;effects[Buffet].releaseMs=120;effects[Buffet].threshold=.04f;effects[Buffet].settleMs=260;effects[Buffet].coastMs=0;
    effects[Taxi].enabled=false;effects[Taxi].motorMax=12;effects[Taxi].gain=.55f;effects[Taxi].attackMs=50;effects[Taxi].releaseMs=80;effects[Taxi].settleMs=200;effects[Taxi].coastMs=0;
    effects[Afterburner].enabled=true;effects[Afterburner].motorMin=12;effects[Afterburner].motorMax=21;effects[Afterburner].gain=1;effects[Afterburner].threshold=0;effects[Afterburner].attackMs=0;effects[Afterburner].releaseMs=30;effects[Afterburner].holdMs=180;effects[Afterburner].settleMs=700;effects[Afterburner].coastMs=350;effects[Afterburner].cooldownMs=1400;
    effects[Stores].enabled=true;effects[Stores].attackMs=0;effects[Stores].releaseMs=20;effects[Stores].threshold=0;effects[Stores].holdMs=160;effects[Stores].settleMs=0;effects[Stores].coastMs=280;effects[Stores].cooldownMs=440;
    effects[Countermeasures].enabled=true;effects[Countermeasures].gain=1;effects[Countermeasures].threshold=0;effects[Countermeasures].attackMs=0;effects[Countermeasures].releaseMs=20;effects[Countermeasures].holdMs=140;effects[Countermeasures].settleMs=0;effects[Countermeasures].coastMs=260;effects[Countermeasures].cooldownMs=400;
    effects[Airflow].enabled=true;effects[Airflow].motorMin=11;effects[Airflow].motorMax=13;effects[Airflow].gain=1;effects[Airflow].attackMs=180;effects[Airflow].releaseMs=120;effects[Airflow].settleMs=320;effects[Airflow].coastMs=0;
    effects[Engine].motorMin=10;effects[Engine].motorMax=12;effects[Engine].gain=.7f;effects[Engine].attackMs=400;effects[Engine].releaseMs=160;effects[Engine].settleMs=450;effects[Engine].coastMs=0;
    effects[AfterburnerRumble].motorMin=12;effects[AfterburnerRumble].motorMax=16;effects[AfterburnerRumble].gain=1;effects[AfterburnerRumble].threshold=0;effects[AfterburnerRumble].priority=30;effects[AfterburnerRumble].attackMs=250;effects[AfterburnerRumble].releaseMs=120;effects[AfterburnerRumble].settleMs=300;effects[AfterburnerRumble].coastMs=0;
    effects[Catapult].gain=1;effects[Catapult].priority=95;effects[Catapult].threshold=0;
    effects[Catapult].motorMin=20;effects[Catapult].motorMax=25;
    effects[Catapult].attackMs=30;effects[Catapult].releaseMs=60;effects[Catapult].settleMs=100;
}
Json Settings::json() const {
    Json j={{"version",1},{"mixRevision",1},{"profile",profile},{"muted",muted},
        {"master",master},{"motorFloor",motorFloor},{"motorCap",motorCap},{"motorCurve",motorCurve},{"staleMs",staleMs},{"dcsProfiles",dcsProfiles},{"toolkitPath",toolkitPath},
        {"gearStartMs",gearStartMs},{"gearLockMs",gearLockMs},{"gearDemoSeconds",gearDemoSeconds},
        {"gearStartGapMs",gearStartGapMs},{"gearLockGapMs",gearLockGapMs},{"gearRampMs",gearRampMs}};
    for(size_t i=0;i<effectCount;++i) { const auto& e=effects[i];
        j["effects"][effectKeys[i]]={{"enabled",e.enabled},{"gain",e.gain},{"threshold",e.threshold},{"curve",e.curve},
            {"attackMs",e.attackMs},{"releaseMs",e.releaseMs},{"motorMin",e.motorMin},{"motorMax",e.motorMax},{"priority",e.priority},
            {"holdMs",e.holdMs},{"settleMs",e.settleMs},{"coastMs",e.coastMs},{"cooldownMs",e.cooldownMs}};
    }
    j["activeAircraft"]=activeAircraft;
    j["aircraftTuning"]=aircraftTuning;
    j["aircraftTuning"][activeAircraft]=tuning(j);
    return j;
}
Settings Settings::fromJson(const Json& j) {
    if(!j.is_object() || j.value("version",0)!=1) throw std::runtime_error("Unsupported profile version");
    Settings s; s.profile=j.value("profile",s.profile); if(s.profile.size()>100) s.profile.resize(100);
    s.activeAircraft=j.value("activeAircraft",std::string("hornet"));
    if(!profileById(s.activeAircraft))s.activeAircraft="hornet";
    if(j.contains("aircraftTuning") && j["aircraftTuning"].is_object())
        for(const auto& p:aircraftProfiles())
            if(j["aircraftTuning"].contains(p.id) && j["aircraftTuning"][p.id].is_object())
                s.aircraftTuning[p.id]=tuning(j["aircraftTuning"][p.id]);
    // Obsolete startup and activation flags are ignored.
    s.muted=j.value("muted",false);
    auto number=[](const Json& obj,const char* key,float fallback,float lo,float hi) {
        float n=obj.value(key,fallback); return std::isfinite(n)?std::clamp(n,lo,hi):fallback; };
    s.master=number(j,"master",s.master,0,1); s.motorCap=std::clamp(j.value("motorCap",s.motorCap),10,25);
    s.motorFloor=std::clamp(j.value("motorFloor",10),10,s.motorCap);
    s.motorCurve=number(j,"motorCurve",1.f,.25f,3.f);
    s.gearStartMs=std::clamp(j.value("gearStartMs",s.gearStartMs),50,1000);
    s.gearLockMs=std::clamp(j.value("gearLockMs",s.gearLockMs),50,1000);
    s.gearStartGapMs=std::clamp(j.value("gearStartGapMs",s.gearStartGapMs),0,1500);
    s.gearLockGapMs=std::clamp(j.value("gearLockGapMs",s.gearLockGapMs),0,1500);
    s.gearRampMs=std::clamp(j.value("gearRampMs",s.gearRampMs),0,3000);
    s.gearDemoSeconds=number(j,"gearDemoSeconds",s.gearDemoSeconds,3,15);
    s.staleMs=std::clamp(j.value("staleMs",300),150,1000);
    s.toolkitPath=j.value("toolkitPath",std::string{}); s.dcsProfiles=j.value("dcsProfiles",std::vector<std::string>{});
    if(j.contains("effects")) for(size_t i=0;i<effectCount;++i) {
        if(!j["effects"].contains(effectKeys[i])) continue;
        const auto& v=j["effects"][effectKeys[i]]; auto& e=s.effects[i];
        e.enabled=effectSupported(i) && v.value("enabled",e.enabled); e.gain=number(v,"gain",e.gain,0,2);
        e.threshold=number(v,"threshold",e.threshold,0,.9f); e.curve=number(v,"curve",e.curve,.2f,3);
        e.attackMs=number(v,"attackMs",e.attackMs,0,500); e.releaseMs=number(v,"releaseMs",e.releaseMs,20,1000);
        e.motorMin=std::clamp(v.value("motorMin",e.motorMin),10,25);
        e.motorMax=std::clamp(v.value("motorMax",e.motorMax),e.motorMin,25);
        e.priority=std::clamp(v.value("priority",e.priority),0,100);
        e.holdMs=std::clamp(v.value("holdMs",e.holdMs),80,1000);
        e.settleMs=std::clamp(v.value("settleMs",e.settleMs),0,1000);
        e.coastMs=std::clamp(v.value("coastMs",e.coastMs),0,1500);
        e.cooldownMs=std::clamp(v.value("cooldownMs",e.cooldownMs),0,3000);
    }
    // Migrate the old default once, without repeatedly overriding user tuning.
    if(j.value("mixRevision",0)<1 && s.effects[Buffet].priority==65)s.effects[Buffet].priority=Settings{}.effects[Buffet].priority;
    return s;
}
bool Settings::selectAircraft(const std::string& id){
    const auto* aircraft=profileById(id);
    if(!aircraft || id==activeAircraft)return false;
    auto next=json(); // Snapshot current tuning before changing aircraft.
    auto saved=next["aircraftTuning"];
    Json selected;
    if(saved.contains(id))selected=saved[id];
    else {
        Settings defaults;
        defaults.profile=std::string(aircraft->name)+" - Headset essentials";
        if(!aircraft->afterburner){defaults.effects[Afterburner].enabled=false;defaults.effects[AfterburnerRumble].enabled=false;}
        defaults.effects[Catapult].enabled=aircraft->carrier;
        if(aircraft->helicopter){defaults.effects[Gear].enabled=false;defaults.effects[Airflow].enabled=false;}
        fitEffectRanges(defaults,motorFloor,motorCap);
        selected=tuning(defaults.json());
    }
    for(auto it=selected.begin();it!=selected.end();++it)next[it.key()]=it.value();
    next["activeAircraft"]=id;next["aircraftTuning"]=saved;
    *this=fromJson(next);
    return true;
}
int mapMotor(float level,int minimum,int maximum,int cap,float curve) {
    if(!std::isfinite(level) || level<=.015f) return 0;
    minimum=std::clamp(minimum,10,25); maximum=std::clamp(maximum,minimum,25); cap=std::clamp(cap,10,25);
    minimum=std::min(minimum,cap);maximum=std::min(maximum,cap);
    curve=std::isfinite(curve)?std::clamp(curve,.25f,3.f):1.f;
    // This is an adjustable command curve, not a measured force/frequency model.
    const float response=std::pow(std::clamp(level,0.f,1.f),curve);
    return minimum+int(std::round(response*(maximum-minimum)));
}
void fitEffectRanges(Settings& s,int minimum,int maximum) {
    minimum=std::clamp(minimum,10,25);maximum=std::clamp(maximum,minimum,25);
    int oldMin=25,oldMax=10;
    for(const auto& e:s.effects){oldMin=std::min(oldMin,std::min(e.motorMin,s.motorCap));oldMax=std::max(oldMax,std::min(e.motorMax,s.motorCap));}
    auto remap=[&](int value){const float fraction=oldMax>oldMin?float(std::clamp(value,oldMin,oldMax)-oldMin)/(oldMax-oldMin):0.f;
        return minimum+int(std::round(fraction*(maximum-minimum)));};
    for(auto& e:s.effects){e.motorMin=remap(e.motorMin);e.motorMax=remap(e.motorMax);}
    s.motorFloor=minimum;s.motorCap=maximum;
}
void applyGearPreset(Settings& s){
    const Settings defaults;s.effects[Gear]=defaults.effects[Gear];
    s.gearStartMs=defaults.gearStartMs;s.gearLockMs=defaults.gearLockMs;
    s.gearStartGapMs=defaults.gearStartGapMs;s.gearLockGapMs=defaults.gearLockGapMs;s.gearRampMs=defaults.gearRampMs;
    s.gearDemoSeconds=4.8f; // Preset audition duration; live travel follows DCS.
}
void applyHeadsetMix(Settings& s){
    const auto gear=s.effects[Gear];s.effects=Settings{}.effects;s.effects[Gear]=gear;
    const auto* p=profileById(s.activeAircraft);
    s.effects[Catapult].enabled=p && p->carrier;
    if(p && !p->afterburner){s.effects[Afterburner].enabled=false;s.effects[AfterburnerRumble].enabled=false;}
    if(p && p->helicopter){s.effects[Gear].enabled=false;s.effects[Airflow].enabled=false;}
    s.profile=std::string(p?p->name:"Aircraft")+" - Headset essentials"; // Preserve the validated gear rhythm and local calibration.
}
void EffectEngine::reset() {
    previous_.reset(); targets_.fill(0); levels_.fill(0); available_.fill(false);
    pendingAt_.fill(0);consumedAt_.fill(0);beganAt_.fill(0);lastEventAt_.fill(0);offAt_.fill(0);pendingStrength_.fill(0);eventStrength_.fill(0);
    freshAt_=lastTick_=holdUntil_=0; held_=-1; afterburnerOn_=false;
    gearMoving_=false;gearDirection_=0;gearBegan_=gearChanged_=gearLockedAt_=0;
    airborneAt_=0;taxiBaseline_.reset();
    catapultArmedAt_=catapultAccelAt_=catapultStart_=0;catapultSpent_=catapultReady_=false;
}
void EffectEngine::trigger(size_t i,uint64_t now,float strength) {
    if(!effectSupported(i)) return;
    available_[i]=true;pendingAt_[i]=now;pendingStrength_[i]=std::clamp(strength,0.f,1.f);freshAt_=now;
}
void EffectEngine::ingest(const Frame& f,uint64_t now,int maxGapMs) {
    const auto* aircraft=aircraftProfile(f.aircraft);
    if(f.state!="flying" || !aircraft) {reset(); return;}
    if(previous_ && (f.session!=previous_->session || f.aircraft!=previous_->aircraft || f.simTime<previous_->simTime || now-freshAt_>uint64_t(maxGapMs))) reset();
    if(previous_ && (f.simTime<=previous_->simTime || f.sequence<=previous_->sequence)) return;
    freshAt_=now;
    auto val=[&](const char* k){return f.value(k);};
    auto prev=[&](const char* k)->std::optional<double>{return previous_?previous_->value(k):std::nullopt;};
    auto drop=[&](const char* k) { auto a=val(k),b=prev(k); return a && b && *a>=0 && *a<*b; };
    const auto ground=val("on_ground"), oldGround=prev("on_ground"), speed=val("ground_mps"), accel=val("accel_y_g");
    available_[Gun]=bool(val("cannon_rounds"));
    if(drop("cannon_rounds")) trigger(Gun,now,1);
    available_[Buffet]=val("shake") && ground;
    targets_[Buffet]=available_[Buffet] && *ground<.5 ? float(std::clamp(*val("shake"),0.,1.)) : 0;
    available_[Touchdown]=ground && val("vertical_mps");
    if(ground && oldGround && *ground>.5 && *oldGround<.5 && airborneAt_ && now-airborneAt_>=500 && prev("vertical_mps") && *prev("vertical_mps")<=-.3048)
        trigger(Touchdown,now,float(std::clamp(std::abs(*prev("vertical_mps"))/2.54,0.,1.)));
    if(ground && *ground<.5){if(!airborneAt_)airborneAt_=now;}else airborneAt_=0;
    available_[Taxi]=ground && speed && accel;
    targets_[Taxi]=0;
    if(available_[Taxi] && *ground>.5 && *speed>1){
        if(!taxiBaseline_)taxiBaseline_=*accel;
        const double dt=previous_?std::clamp(f.simTime-previous_->simTime,0.,.1):.02;
        *taxiBaseline_+=(*accel-*taxiBaseline_)*(1-std::exp(-dt/.6));
        targets_[Taxi]=float(std::clamp(std::abs(*accel-*taxiBaseline_)*2,0.,1.));
    }else taxiBaseline_.reset();
    const auto gear=aircraft->helicopter?std::optional<double>{}:val("gear"),brake=aircraft->helicopter?std::optional<double>{}:val("airbrake"),ias=val("ias_mps");
    available_[Airflow]=ground && ias && (gear || brake);
    // Approximate configuration airflow from deployment and airspeed, separately from gear transit.
    targets_[Airflow]=available_[Airflow] && *ground<.5 ? float(std::clamp((std::max(gear.value_or(0),brake.value_or(0)))*(*ias-40)/100,0.,1.)) : 0;
    available_[Damage]=false; // Animation damage is not a reliable individual hit event.
    available_[Gear]=bool(gear);
    if(gear && prev("gear")){
        const double delta=*gear-*prev("gear");
        if(std::abs(delta)>.0005){
            const int direction=delta>0?1:-1;
            if(!gearMoving_ || direction!=gearDirection_){gearBegan_=now;gearLockedAt_=0;}
            gearMoving_=true;gearDirection_=direction;gearChanged_=now;
            if((direction<0 && *gear<=.002) || (direction>0 && *gear>=.998)){
                gearMoving_=false;gearLockedAt_=now;
            }
        }else if(gearMoving_ && now-gearChanged_>300)gearMoving_=false;
    }else{gearMoving_=false;gearLockedAt_=0;}
    auto ab=aircraft->afterburner?val("ab_left"):std::optional<double>{};
    auto ab2=aircraft->engines==1?ab:val("ab_right");
    available_[Afterburner]=available_[AfterburnerRumble]=ab && ab2;
    targets_[AfterburnerRumble]=0;
    if(ab && ab2) {
        const auto amount=std::max(*ab,*ab2);
        if(!prev("ab_left") || (aircraft->engines>1 && !prev("ab_right")))afterburnerOn_=amount>.05;
        else if(!afterburnerOn_ && amount>.15){trigger(Afterburner,now,1.f);afterburnerOn_=true;}
        else if(amount<.05)afterburnerOn_=false;
        if(afterburnerOn_)targets_[AfterburnerRumble]=float(std::clamp(amount,0.,1.));
    }else afterburnerOn_=false;
    available_[Stores]=val("stores_count") && ground; if(ground && *ground<.5 && drop("stores_count")) trigger(Stores,now,1.f);
    auto rpm=val("rpm_left_pct"),rpm2=aircraft->engines==1?rpm:val("rpm_right_pct"); available_[Engine]=rpm && rpm2;
    targets_[Engine]=rpm && rpm2 ? float(std::clamp((std::max(*rpm,*rpm2)-40)/60,0.,1.)) : 0;
    available_[Countermeasures]=bool(val("flares")) || bool(val("chaff"));
    if(drop("flares") || drop("chaff")) trigger(Countermeasures,now,1.f);
    const auto launchBar=val("launch_bar"), forwardG=val("accel_x_g");
    available_[Catapult]=aircraft->carrier && ground && speed && launchBar && forwardG;
    targets_[Catapult]=0;
    if(available_[Catapult]){
        if(*launchBar<.1 && *speed<20 && catapultSpent_){catapultSpent_=catapultReady_=false;catapultArmedAt_=0;}
        if(!catapultSpent_ && *ground>.5 && *launchBar>.5 && *speed<25 && std::abs(*forwardG)<.3){
            if(!catapultArmedAt_)catapultArmedAt_=now;
            if(now-catapultArmedAt_>=500)catapultReady_=true;
        }else if(!catapultReady_)catapultArmedAt_=0;
        if(catapultArmedAt_ && now-catapultArmedAt_>30000){catapultReady_=false;catapultArmedAt_=0;}
        // Require an observed low-acceleration pre-launch baseline; reconnecting mid-shot cannot fire.
        const bool accelerating=catapultReady_ &&
            *ground>.5 && *forwardG>1.5 && prev("ground_mps") && *speed>*prev("ground_mps");
        if(accelerating && !catapultSpent_){
            if(!catapultAccelAt_)catapultAccelAt_=now;
            if(now-catapultAccelAt_>=100){catapultStart_=now;catapultSpent_=true;}
        }else catapultAccelAt_=0;
        if(catapultStart_ && now-catapultStart_<4000 && *ground>.5 && *forwardG>1.0)
            targets_[Catapult]=1;
        else if(catapultStart_)catapultStart_=0;
        if(!catapultStart_ && *ground<.5){catapultArmedAt_=0;catapultReady_=false;}
    }else{catapultArmedAt_=catapultAccelAt_=catapultStart_=0;catapultSpent_=catapultReady_=false;}
    previous_=f;
}
Mix EffectEngine::tick(uint64_t now,const Settings& s,bool demo) {
    Mix m;
    const float dt=lastTick_?float(std::min<uint64_t>(now-lastTick_,10000)):20; lastTick_=now;
    const bool fresh=freshAt_ && (demo || now-freshAt_<=uint64_t(s.staleMs));
    int chosen=-1; float best=-1;
    for(size_t i=0;i<effectCount;++i) {
        const auto& c=s.effects[i]; auto& v=m.effects[i]; v.available=available_[i];
        if(!effectSupported(i)){levels_[i]=0;v.available=false;v.reason="Deferred: event not verified";continue;}
        const auto shape=effectDefinition(i).shape;
        const bool eventCue=shape==CueShape::Burst || shape==CueShape::Impact || shape==CueShape::Surge;
        bool forceZero=false;const char* eventPhase=nullptr;
        float target=fresh && v.available?targets_[i]:0;
        if(eventCue){
            if(pendingAt_[i]!=consumedAt_[i]){
                const auto eventTime=pendingAt_[i];consumedAt_[i]=eventTime;
                const auto recovery=uint64_t(std::max(c.cooldownMs,c.holdMs+c.settleMs+c.coastMs));
                if(shape==CueShape::Burst || !beganAt_[i] || eventTime-beganAt_[i]>=recovery){
                    if(shape!=CueShape::Burst || !beganAt_[i] || eventTime-lastEventAt_[i]>uint64_t(c.holdMs+c.settleMs))beganAt_[i]=eventTime;
                    lastEventAt_[i]=eventTime;eventStrength_[i]=pendingStrength_[i];
                }
                // Events inside recovery are consumed, never queued for later playback.
            }
            target=0;forceZero=true;
            if(!fresh || !v.available)beganAt_[i]=0;
            if(fresh && v.available && beganAt_[i]){
                const auto age=now-(shape==CueShape::Burst?lastEventAt_[i]:beganAt_[i]);
                if(age<uint64_t(c.holdMs)){
                    target=eventStrength_[i];forceZero=false;
                    eventPhase=shape==CueShape::Burst?"Gun burst":shape==CueShape::Surge?"Ignition kick":"Sharp impact";
                }else if(age<uint64_t(c.holdMs+c.settleMs)){
                    const float elapsed=float(age-c.holdMs),remaining=1-elapsed/std::max(1,c.settleMs);
                    target=eventStrength_[i]*(shape==CueShape::Surge?
                        (.33f+.10f*std::sin(elapsed*.009f))*std::min(1.f,remaining*c.settleMs/180.f):remaining);
                    forceZero=false;eventPhase=shape==CueShape::Surge?"Ignition rumble":"Impact settling";
                }else if(age<uint64_t(c.holdMs+c.settleMs+c.coastMs) && eventStrength_[i]>0){eventPhase="Recovery / background resumes";}
            }
        }else if(shape==CueShape::Continuous){
            if(target<=c.threshold){if(!offAt_[i])offAt_[i]=now;forceZero=now-offAt_[i]>=uint64_t(c.settleMs);}
            else offAt_[i]=0;
        }
        const bool gearSequence=i==Gear && previous_ && fresh;
        bool gearGap=false,gearTravel=false;const char* gearPhase=nullptr;
        if(gearSequence){
            target=0;
            if(gearLockedAt_){
                const auto elapsed=now-gearLockedAt_;
                if(elapsed<uint64_t(s.gearLockGapMs)){gearGap=true;gearPhase="Coast / quiet before lock";}
                else if(elapsed<uint64_t(s.gearLockGapMs+s.gearLockMs)){target=5.f/6.f;gearPhase="Lock THUNK";}
            }else if(gearMoving_){
                const auto elapsed=now-gearBegan_;
                if(elapsed<uint64_t(s.gearStartMs)){target=1;gearPhase="Start THUNK";}
                else if(elapsed<uint64_t(s.gearStartMs+s.gearStartGapMs)){gearGap=true;gearPhase="Coast / quiet after start";}
                else{
                    const float travelMs=float(elapsed-s.gearStartMs-s.gearStartGapMs);
                    const float ramp=s.gearRampMs?std::min(travelMs/s.gearRampMs,1.f):1.f;
                    target=.02f+ramp*(.16f+.15f*std::sin(travelMs*.01f));
                    gearTravel=true;gearPhase=ramp<1?"Travel rumble rising":"Travel rumble";
                }
            }
        }
        const float tau=target>levels_[i]?c.attackMs:c.releaseMs;
        levels_[i]+=(target-levels_[i])*(tau<=0?1.f:1-std::exp(-dt/tau));
        // The physical motor already coasts down. Do not prolong deliberate
        // quiet phases with a software tail that still maps to motorMin.
        if(!fresh || !v.available || forceZero || (gearSequence && target==0)) levels_[i]=0;
        const float scaled=std::pow(std::clamp((levels_[i]-c.threshold)/(1-c.threshold),0.f,1.f),c.curve)*c.gain*s.master;
        v.level=c.enabled?std::clamp(scaled,0.f,1.f):0;
        // A positively enabled motion bed stays continuous, even at low gain.
        // Mute, zero gain/master, disabled effects and stale telemetry still stop it.
        if(gearTravel && c.enabled && c.gain>0 && s.master>0)v.level=std::max(v.level,.02f);
        v.reason=!c.enabled?"Disabled":!fresh?"Waiting for fresh telemetry":!v.available?"Signal unavailable":v.level<.02f?"Below threshold":"Active";
        const bool reserveGap=gearGap && c.enabled && c.gain>0 && s.master>0 && fresh && v.available;
        if(gearPhase && (v.level>=.02f || reserveGap))v.reason=gearPhase;
        if(eventPhase && (v.level>=.02f || reserveGap))v.reason=eventPhase;
        const float score=float(c.priority)+v.level;
        // Keep lower-priority airflow/ambience out of the gear's quiet gaps.
        // Higher-priority gunfire, touchdown and other events can still win.
        if((v.level>=.02f || reserveGap) && score>best){chosen=int(i);best=score;}
    }
    if(held_>=0 && now<holdUntil_ && m.effects[held_].level>=.02f && chosen>=0 && s.effects[held_].priority>=s.effects[chosen].priority) chosen=held_;
    if(chosen!=held_) {held_=chosen;holdUntil_=now+80;}
    if(chosen>=0){m.dominant=chosen;m.intensity=m.effects[chosen].level;
        const auto& e=s.effects[chosen];m.motor=mapMotor(m.intensity,e.motorMin,e.motorMax,s.motorCap,s.motorCurve);
        for(size_t i=0;i<effectCount;++i)if(int(i)!=chosen && m.effects[i].level>=.02f)m.effects[i].reason="Background suppressed";
    }
    return m;
}
std::vector<FlightDemoStage> flightDemoStages(const Settings& s){
    std::vector<FlightDemoStage> stages;double next=0;
    auto add=[&](FlightDemoStep step,double duration,const char* title,const char* description){
        stages.push_back({step,next,duration,title,description});next+=duration;
    };
    const auto tail=[&](size_t i){const auto& c=s.effects[i];return (c.holdMs+c.settleMs+c.coastMs)/1000.+.4;};
    const double travel=std::clamp(double(s.gearDemoSeconds),3.,15.);
    const double gearDuration=travel+gearDemoRestSeconds(s)+.1;
    add(FlightDemoStep::Roll,3,"Ground roll","Engine ambience and runway bumps; quiet when both are disabled.");
    add(FlightDemoStep::GearUp,gearDuration,"Takeoff / gear up","Start THUNK, quiet gap, travel rumble, coast and lock THUNK.");
    add(FlightDemoStep::Gunfire,1.2+tail(Gun),"Gun burst","Sustained firing, then a short settle and quiet recovery.");
    add(FlightDemoStep::Buffet,3+s.effects[Buffet].settleMs/1000.,"Airborne buffet","A rising and falling airframe shake.");
    add(FlightDemoStep::Airbrake,3+s.effects[Airflow].settleMs/1000.,"Airbrake airflow","Subtle airflow with the speedbrake extended.");
    add(FlightDemoStep::Afterburner,4+tail(Afterburner),"Afterburner","Ignition kick and recovery, then sustained rumble while engaged.");
    add(FlightDemoStep::Stores,tail(Stores),"Store release","One sharp release pulse.");
    add(FlightDemoStep::Countermeasures,tail(Countermeasures),"Countermeasures","One short dispense pulse.");
    add(FlightDemoStep::GearDown,gearDuration,"Approach / gear down","The same mechanism rhythm, with subtle gear airflow.");
    add(FlightDemoStep::Landing,tail(Touchdown),"Touchdown","A landing thud, settle and quiet recovery.");
    add(FlightDemoStep::Rest,.5,"Finish","The demo stops; available DCS flight output resumes.");
    return stages;
}
int flightDemoStageAt(const std::vector<FlightDemoStage>& stages,double t){
    for(size_t i=0;i<stages.size();++i)if(t>=stages[i].start && t<stages[i].start+stages[i].duration)return int(i);
    return -1;
}
Frame flightDemoFrame(double t,uint64_t now,const std::vector<FlightDemoStage>& stages,double gearTravelSeconds){
    auto start=[&](FlightDemoStep step){for(const auto& s:stages)if(s.step==step)return s.start;return 1e9;};
    const auto up=start(FlightDemoStep::GearUp),down=start(FlightDemoStep::GearDown),landing=start(FlightDemoStep::Landing);
    const double travel=std::clamp(gearTravelSeconds,3.,15.);
    const double gear=t<up?1:t<down?1-std::clamp((t-up)/travel,0.,1.):std::clamp((t-down)/travel,0.,1.);
    const bool airborne=t>=up && t<landing;
    const double gunTime=std::clamp(t-start(FlightDemoStep::Gunfire),0.,1.2);
    const double buffetTime=t-start(FlightDemoStep::Buffet),brakeTime=t-start(FlightDemoStep::Airbrake);
    Frame f;f.session=1;f.sequence=uint64_t(t*50)+1;f.simTime=t;f.receivedMs=now;f.state="flying";f.aircraft="FA-18C_hornet";
    f.values={{"on_ground",airborne?0:1},{"ground_mps",t<up?20:airborne?100:0},{"ias_mps",airborne?140:0},
        {"gear",gear},{"airbrake",brakeTime>=0&&brakeTime<3?1:0},{"vertical_mps",airborne&&t>=down?-4.5:0},
        {"accel_y_g",1+(t<up?.2*std::sin(t*9):0)},{"shake",buffetTime>=0&&buffetTime<3?.6*std::sin(buffetTime/3*3.141592653589793):0},
        {"cannon_rounds",500-std::floor(gunTime*50)},{"rpm_left_pct",t<landing?80:0},{"rpm_right_pct",t<landing?80:0},
        {"ab_left",t>=start(FlightDemoStep::Afterburner)&&t<down?.6:0},{"ab_right",t>=start(FlightDemoStep::Afterburner)&&t<down?.6:0},
        {"stores_count",t>=start(FlightDemoStep::Stores)?3:4},{"flares",t>=start(FlightDemoStep::Countermeasures)?29:30},{"chaff",60},{"damage_total",0}};
    return f;
}
Frame effectDemoFrame(size_t i,double t,uint64_t now){
    Frame f;f.session=3+i;f.sequence=uint64_t(t*50)+1;f.simTime=t;f.receivedMs=now;f.state="flying";f.aircraft="FA-18C_hornet";
    const double rise=std::clamp((t-.4)/1.2,0.,1.),fall=std::clamp((3.3-t)/.9,0.,1.);
    const double bed=std::min(rise,fall);
    switch(i){
    case Catapult:f.values={{"on_ground",t<2.9?1:0},{"launch_bar",1},{"ground_mps",t<.8?10:10+(t-.8)*25},{"accel_x_g",t>=.8&&t<2.9?2.5:0}};break;
    case Gun:f.values={{"cannon_rounds",500-(t>.4?std::min(70.,std::floor((t-.4)*70)):0)-(t>2.3?std::min(35.,std::floor((t-2.3)*70)):0)}};break;
    case Touchdown:f.values={{"on_ground",t<.8?0:1},{"vertical_mps",t<.8?-4.5:0}};break;
    case Buffet:f.values={{"on_ground",0},{"shake",bed*(.55+.18*std::sin(t*6))}};break;
    case Taxi:f.values={{"on_ground",1},{"ground_mps",t<3.3?20:0},{"accel_y_g",1+bed*.3*std::sin(t*11)}};break;
    case Afterburner:f.values={{"ab_left",t<.4?0:.6},{"ab_right",t<.55?0:.6}};break;
    case AfterburnerRumble:f.values={{"ab_left",t>=.4&&t<3.3?.2+.8*bed:0},{"ab_right",t>=.55&&t<3.3?.2+.7*bed:0}};break;
    case Stores:f.values={{"on_ground",0},{"stores_count",t<.4?4:3}};break;
    case Countermeasures:f.values={{"flares",t<.4?30:t<.55?29:28},{"chaff",60}};break;
    case Engine:f.values={{"rpm_left_pct",40+bed*60},{"rpm_right_pct",40+bed*57}};break;
    case Airflow:f.values={{"on_ground",0},{"gear",1},{"airbrake",.5},{"ias_mps",40+bed*100}};break;
    default:break;
    }return f;
}
double effectDemoDuration(size_t i,const Settings& s){
    if(!effectSupported(i))return 0;
    if(i==Gear)return gearDemoDuration(s.gearDemoSeconds,gearDemoRestSeconds(s));
    const auto& c=s.effects[i];const auto shape=effectDefinition(i).shape;
    if(shape==CueShape::Continuous)return 3.3+c.settleMs/1000.+.4;
    return (i==Gun?2.8:i==Touchdown?.8:.4)+(c.holdMs+c.settleMs+c.coastMs)/1000.+.4;
}
double gearDemoRestSeconds(const Settings& s){return (s.gearLockGapMs+s.gearLockMs+390)/1000.;}
double gearDemoDuration(double travelSeconds,double restSeconds){return .4+2*(std::clamp(travelSeconds,3.,15.)+std::clamp(restSeconds,.39,3.));}
Frame gearDemoFrame(double t,uint64_t now,double travelSeconds,double restSeconds){
    Frame f;f.session=2;f.sequence=uint64_t(t*50)+1;f.simTime=t;f.receivedMs=now;f.state="flying";f.aircraft="FA-18C_hornet";
    const double stroke=std::clamp(travelSeconds,3.,15.);
    const double downStart=.4+stroke+std::clamp(restSeconds,.39,3.);
    const double gear=t<.4?1:t<.4+stroke?1-(t-.4)/stroke:t<downStart?0:t<downStart+stroke?(t-downStart)/stroke:1;
    f.values={{"gear",gear},{"on_ground",1},{"ias_mps",0}};return f;
}
}
