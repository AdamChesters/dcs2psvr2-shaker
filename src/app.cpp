#include "app.hpp"
#include "telemetry.hpp"
#include "haptics.hpp"
#include "integration.hpp"
#include "version.hpp"
#include <imgui.h>
#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string_view>

namespace shaker {
namespace {
fs::path chooseFile(bool save,const wchar_t* filter,const wchar_t* extension){
    wchar_t file[32768]{};OPENFILENAMEW of{sizeof(of)};of.lpstrFile=file;of.nMaxFile=32768;
    of.lpstrFilter=filter;of.lpstrDefExt=extension;of.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    return (save?GetSaveFileNameW(&of):GetOpenFileNameW(&of))?fs::path(file):fs::path{};
}
void mutedText(const char* s){
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",s);ImGui::PopStyleColor();
}
void continueRow(const char* nextLabel,float fixedWidth=0){
    const float width=fixedWidth>0?fixedWidth:ImGui::CalcTextSize(nextLabel).x+2*ImGui::GetStyle().FramePadding.x;
    const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
    if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=right)ImGui::SameLine();
}
template<class Draw> bool labeledControl(const char* label,Draw draw){
    bool changed=false;ImGui::PushID(label);
    if(ImGui::BeginTable("ControlRow",2,ImGuiTableFlags_SizingStretchProp)){
        ImGui::TableSetupColumn("Label",ImGuiTableColumnFlags_WidthStretch,.34f);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,.66f);
        ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextWrapped("%s",label);
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);changed=draw();ImGui::EndTable();
    }
    ImGui::PopID();return changed;
}
bool tuningInt(const char* label,int* value,int minimum,int maximum,const char* format="%d"){
    return labeledControl(label,[&]{return ImGui::SliderInt("##Value",value,minimum,maximum,format);});
}
bool tuningFloat(const char* label,float* value,float minimum,float maximum,const char* format="%.2f"){
    return labeledControl(label,[&]{return ImGui::SliderFloat("##Value",value,minimum,maximum,format);});
}
bool statusLight(const char* label,const std::string& detail,bool ready,const std::string& help,bool attention=false,bool tintText=false){
    const auto color=attention?ImVec4(1.f,.78f,.30f,1):ready?ImVec4(.38f,.83f,.60f,1):ImVec4(.55f,.59f,.62f,1);
    const auto lamp=attention&&std::fmod(ImGui::GetTime(),1.2)>=.6?ImVec4(.28f,.23f,.12f,1):color;
    ImGui::BeginGroup();
    const auto p=ImGui::GetCursorScreenPos();const float height=ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled({p.x+5,p.y+height*.5f},4,ImGui::GetColorU32(lamp));
    ImGui::Dummy({10,height});ImGui::SameLine();
    if(tintText)ImGui::PushStyleColor(ImGuiCol_Text,color);
    ImGui::TextWrapped("%s: %s",label,detail.c_str());
    if(tintText)ImGui::PopStyleColor();
    ImGui::EndGroup();const bool clicked=ImGui::IsItemClicked();
    if(ImGui::IsItemHovered()){
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*30);ImGui::TextUnformatted(help.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    return clicked;
}
void activityLight(EffectActivity activity,const std::string& reason){
    const bool priority=activity==EffectActivity::Output||activity==EffectActivity::Gap;
    const bool active=activity==EffectActivity::Active||activity==EffectActivity::Output;
    const char* label=activity==EffectActivity::Output?"Output":activity==EffectActivity::Gap?"Gap":active?"Active":"Idle";
    const auto p=ImGui::GetCursorScreenPos();const float height=ImGui::GetFrameHeight();
    const ImVec2 center{p.x+9,p.y+height*.5f};
    ImGui::BeginGroup();
    ImGui::GetWindowDrawList()->AddCircleFilled(center,4,ImGui::GetColorU32(active?ImVec4(.38f,.83f,.60f,1):ImVec4(.55f,.59f,.62f,1)));
    if(priority){
        const float alpha=.7f+.3f*float(.5+.5*std::sin(ImGui::GetTime()*4));
        ImGui::GetWindowDrawList()->AddCircle(center,8,ImGui::GetColorU32(ImVec4(.55f,.88f,1,alpha)),0,2);
    }
    ImGui::Dummy({20,height});ImGui::SameLine(0,4);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",activity==EffectActivity::Output?"Current motor command. This is not a measurement of vibration.":activity==EffectActivity::Gap?"This cue holds priority during an intentional quiet gap.":reason.c_str());
}
void heading(const char* title,const char* description){
    auto& fonts=ImGui::GetIO().Fonts->Fonts;ImGui::PushFont(fonts.Size>1?fonts[1]:nullptr,34);
    ImGui::TextUnformatted(title);ImGui::PopFont();
    mutedText(description);ImGui::Dummy({0,8});
}
constexpr auto jsonFilter=L"JSON profile\0*.json\0\0";
Json frameJson(const Frame& f){return {{"version",1},{"state",f.state},{"aircraft",f.aircraft},{"values",f.values}};}

}
App::App(){
    try{
        const auto file=dataDirectory()/L"feedback-contact.json";
        if(fs::exists(file)){
            const auto contact=Json::parse(readText(file));
            const auto name=contact.value("name","");const auto email=contact.value("email","");
            if(name.size()<feedbackName_.size())std::copy(name.begin(),name.end(),feedbackName_.begin());
            if(email.size()<feedbackEmail_.size())std::copy(email.begin(),email.end(),feedbackEmail_.begin());
        }
    }catch(...){}
    try{
        const auto marker=dataDirectory()/L"update-state.json";
        std::string lastShown;
        if(fs::exists(marker))try{lastShown=Json::parse(readText(marker)).value("lastShownVersion","");}catch(...){}
        showChangelog_=shouldShowChangelog(lastShown,appVersion,fs::exists(dataDirectory()/L"settings.json"));
        changelogNeedsMark_=showChangelog_;
        if(!showChangelog_ && lastShown.empty())writeTextAtomic(marker,Json({{"lastShownVersion",appVersion}}).dump(2));
    }catch(const std::exception& e){uiMessage_=std::string("Update history: ")+e.what();}
    try{changelog_=latestChangelog(readText(appDirectory()/L"CHANGELOG.md"));}
    catch(...){changelog_="Release notes are unavailable. Open Releases for details.";}
    try{auto p=dataDirectory()/L"settings.json";if(fs::exists(p))settings_=Settings::fromJson(Json::parse(readText(p)));}
    catch(const std::exception& e){uiMessage_=std::string("Settings could not load; using safe defaults. ")+e.what();}
    try{profiles_=dcsProfiles();}catch(const std::exception& e){uiMessage_=e.what();}
    calibrationLow_=settings_.motorFloor;calibrationHigh_=settings_.motorCap;testMotor_=std::min(15,settings_.motorCap);
    worker_=std::jthread([this](std::stop_token stop){run(stop);});
    statusWorker_=std::jthread([this](std::stop_token stop){
        while(!stop.stop_requested()){
            const auto status=readSystemStatus();
            {std::lock_guard lock(mutex_);system_=status;}
            for(int i=0;i<10&&!stop.stop_requested();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}
App::~App(){updates_.cancel();statusWorker_.request_stop();worker_.request_stop();if(statusWorker_.joinable())statusWorker_.join();if(worker_.joinable())worker_.join();try{save();}catch(...){} }
Settings App::settings()const{std::lock_guard lock(mutex_);return settings_;}
Snapshot App::snapshot()const{std::lock_guard lock(mutex_);auto result=snapshot_;result.system=system_;return result;}
void App::command(Command c){std::lock_guard lock(mutex_);if(c.action==Action::LabPlay)settings_.muted=false;commands_.push_back(std::move(c));}
void App::save(){writeTextAtomic(dataDirectory()/L"settings.json",settings().json().dump(2));settingsDirty_=false;}
void App::emergencyStop(){
    {std::lock_guard lock(mutex_);settings_.muted=true;commands_.push_back({Action::Stop});}
    settingsDirty_=true;saveAt_=GetTickCount64()+200;
}
void App::renderSetup(){
    if(showSetup_){ImGui::OpenPopup("Setup");showSetup_=false;}
    const auto screen=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(screen->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSize({std::min(700.f,screen->Size.x-48),std::min(620.f,screen->Size.y-48)},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Setup",nullptr,ImGuiWindowFlags_NoCollapse)){
        ImGui::BeginChild("SetupSteps",{0,-ImGui::GetFrameHeightWithSpacing()-8});
        const auto link=[](const char* text,const wchar_t* url){
            if(ImGui::Button(text))ShellExecuteW(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
        };
        mutedText("Quick checklist. Follow the linked projects for current requirements and full instructions.");
        ImGui::Spacing();ImGui::SeparatorText("Install once");
        ImGui::TextWrapped("1. Install the newest compatible PSVR2Toolkit release. Check all releases, including experimental builds, and follow its installation guide.");
        link("Toolkit releases",L"https://github.com/BnuuySolutions/PSVR2Toolkit/releases");
        continueRow("Installation guide");link("Installation guide",L"https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation");
        ImGui::TextWrapped("2. Follow the headset jailbreak guide, including firmware requirements. Download and extract vr2jb for Windows.");
        link("Jailbreak guide",L"https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Jailbreaking-your-headset");
        continueRow("vr2jb releases");link("vr2jb releases",L"https://github.com/BnuuySolutions/vr2jb/releases");
        ImGui::TextWrapped("3. In Settings > DCS integration, install the export hook into your DCS profile. Restart DCS if it was open.");
        ImGui::Spacing();ImGui::SeparatorText("After each headset power-on, in this order");
        ImGui::TextWrapped("1. Keep SteamVR and the PlayStation VR2 App closed. Power on the headset, run vr2jb.exe from its extracted folder, and wait for success. If it fails, follow the jailbreak guide before continuing.");
        ImGui::TextWrapped("2. Start SteamVR and wait for the headset to connect.");
        ImGui::TextWrapped("3. Open dcs2psvr2-shaker. Use Tests to check that you feel vibration; the app cannot automatically validate the jailbreak.");
        ImGui::TextWrapped("4. Start DCS and enter a supported aircraft: Hornet, Viper, A-10C/II, Tomcat, Phantom or Apache. Unpause and check the telemetry and aircraft lights. Unmute output.");
        ImGui::Spacing();ImGui::Separator();
        ImGui::TextWrapped("Need help? Open a GitHub issue. A subreddit is planned.");
        link("Open an issue",L"https://github.com/AdamChesters/dcs2psvr2-shaker/issues/new");
        ImGui::EndChild();
        if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void App::renderChangelog(){
    if(showChangelog_){ImGui::OpenPopup("What's new");showChangelog_=false;}
    const auto screen=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(screen->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSize({std::min(640.f,screen->Size.x-48),std::min(440.f,screen->Size.y-48)},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("What's new",nullptr,ImGuiWindowFlags_NoCollapse)){
        if(changelogNeedsMark_){
            changelogNeedsMark_=false;
            try{writeTextAtomic(dataDirectory()/L"update-state.json",Json({{"lastShownVersion",appVersion}}).dump(2));}
            catch(const std::exception& e){uiMessage_=std::string("Could not save update history: ")+e.what();}
        }
        ImGui::Text("%s",appVersionDisplay);ImGui::Separator();
        ImGui::BeginChild("ReleaseNotes",{0,-48});ImGui::TextWrapped("%s",changelog_.c_str());ImGui::EndChild();
        if(ImGui::Button("Continue",{120,0}))ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();ImGui::SameLine();
        if(ImGui::Button("Open Releases"))ShellExecuteW(nullptr,L"open",wide(releasesUrl).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::EndPopup();
    }
}
void App::renderUpdates(){
    if(showUpdates_){ImGui::OpenPopup("Application updates");showUpdates_=false;}
    const auto u=updates_.status();
    const bool available=u.release&&compareVersions(u.release->version,appVersion)>0;
    const bool busy=u.state==UpdateState::Checking||u.state==UpdateState::Downloading;
    const char* state=u.state==UpdateState::Checking?"checking":u.state==UpdateState::Downloading?"downloading":
        u.state==UpdateState::Ready?"ready to install":available?"update":
        u.state==UpdateState::Current?"current":"check unavailable";
    const auto detail=std::string(appVersionDisplay)+" / "+state;
    if(statusLight("Version",detail,u.state==UpdateState::Current,
        "Click for release details and updates. A flashing yellow light means a newer release is available; grey means the version could not be checked.",available,true))
        ImGui::OpenPopup("Application updates");
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    auto install=[&]{
        updateInstallRequested_=false;
        try{
            if(!u.release)throw std::runtime_error("The release could not be verified. Check again.");
            verifyInstaller(u.installer,*u.release);save();
            if(!launchInstaller(u.installer,appDirectory()))throw std::runtime_error("The installer could not be opened. Retry or open Releases.");
            // The normal app shutdown stops the haptics helper and saves tuning.
            // Inno Setup opens visibly and offers to restart the app when finished.
            PostQuitMessage(0);
        }catch(const std::exception& e){updateLaunchError_=e.what();}
    };
    if(u.state==UpdateState::Ready&&updateInstallRequested_)install();
    ImGui::SetNextWindowSize({560,0});
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
    if(ImGui::BeginPopupModal("Application updates",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+500);
        ImGui::Text("Installed: %s",appVersionDisplay);
        if(u.release)ImGui::TextWrapped("Newest published release: %s",u.release->version.c_str());
        if(u.state==UpdateState::Current)ImGui::TextWrapped("You're running the current version. No newer release is published on GitHub.");
        if(u.state==UpdateState::Checking)ImGui::TextWrapped("Checking the project's GitHub releases, including Alpha releases...");
        if(available){
            ImGui::TextWrapped("Download the Windows installer and update this installation. Your saved tuning is retained. dcs2psvr2-shaker will close when the installer opens; select Open dcs2psvr2-shaker at the end to restart it.");
            ImGui::TextWrapped("Close DCS before installing so its export bridge can be updated.");
            if(!u.release->installable())ImGui::TextWrapped("This release has no Windows installer with a GitHub SHA-256 checksum. Open Releases to download it manually.");
        }
        if(!u.message.empty())ImGui::TextWrapped("%s",u.message.c_str());
        if(!updateLaunchError_.empty())ImGui::TextWrapped("%s",updateLaunchError_.c_str());
        if(u.state==UpdateState::Downloading){
            ImGui::ProgressBar(u.release&&u.release->size?float(double(u.downloaded)/double(u.release->size)):0,{-1,0});
            ImGui::TextWrapped("Downloading and verifying the installer...");
            if(ImGui::Button("Cancel download")){updateInstallRequested_=false;updates_.cancel();}
        }else if(u.state==UpdateState::Ready){
            if(ImGui::Button("Open installer"))install();
        }else if(available&&u.release->installable()&&!busy){
            if(ImGui::Button("Download and install")){updateLaunchError_.clear();updateInstallRequested_=true;updates_.download();}
        }
        if(ImGui::Button("Open Releases")){
            const auto page=u.release?u.release->page:std::string(releasesUrl);
            ShellExecuteW(nullptr,L"open",wide(page).c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }
        if(!busy){continueRow("Check again");if(ImGui::Button("Check again")){updateInstallRequested_=false;updateLaunchError_.clear();updates_.check();}}
        continueRow("Installed changelog");if(ImGui::Button("Installed changelog")){showChangelog_=true;ImGui::CloseCurrentPopup();}
        continueRow("Close");if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();ImGui::EndPopup();
    }
}
void App::run(std::stop_token stop){
    TelemetryReader reader;FlightFeed flightFeed;EffectEngine engine;HapticsClient haptics;Frame lastFrame;
    uint64_t lastSession=0,lastSequence=0,rawEnd=0,rawStart=0,rawWait=0,demoStart=0,idleSince=0;
    int rawMotor=0,cueDemo=-1;bool synthetic=false,gearDemo=false,demoWaiting=false,manualConnection=false;
    double gearTravelSeconds=8.,gearRestSeconds=1.1;
    std::vector<FlightDemoStage> timeline;Snapshot view;
    bool labMode=false;int labActive=-1;uint64_t labStart=0,labWait=0;HapticTest labTest;
    auto connect=[&](const Settings& s){if(!haptics.running())haptics.start(s.toolkitPath.empty()?toolkitFile():fs::path(wide(s.toolkitPath)));};
    auto endTest=[&]{
        labActive=-1;labWait=labStart=0;
        rawMotor=0;rawWait=rawEnd=0;cueDemo=-1;synthetic=gearDemo=demoWaiting=false;
        engine.reset();view.demoComplete=false;view.demoSeconds=0;haptics.update(0,GetTickCount64());
    };
    while(!stop.stop_requested()){
        const auto now=GetTickCount64();Settings s;std::vector<Command> commands;
        {std::lock_guard lock(mutex_);s=settings_;commands.swap(commands_);}
        try{
            for(const auto& c:commands)switch(c.action){
            case Action::Connect:
                endTest();haptics.stop();manualConnection=true;connect(s);view.message="Connecting; SteamVR must be running.";break;
            case Action::Stop:
                endTest();view.message="Output stopped. Unmute to resume DCS effects.";break;
            case Action::EndTest:
                endTest();view.message="Test stopped.";break;
            case Action::LabEnter:
                endTest();labMode=true;view.message="";break;
            case Action::LabExit:
                endTest();labMode=false;view.message="";break;
            case Action::LabPlay:
                endTest();labMode=true;
                if(c.value<0 || c.value>=int(hapticPatternNames.size()))break;
                labActive=c.value;labTest=boundedHapticTest(c.test);labWait=now+10000;
                connect(s);view.message="Waiting for headset.";break;
            case Action::Raw:
                endTest();rawMotor=std::clamp(c.value,10,25);rawWait=now+10000;
                connect(s);manualConnection=true;view.message="Waiting for headset, then playing four short vibrations.";break;
            case Action::Effect:case Action::FlightDemo:case Action::GearDemo:
                if(c.action==Action::Effect && (c.value<0 || !effectSupported(size_t(c.value))))break;
                endTest();synthetic=true;demoWaiting=true;gearDemo=c.action==Action::GearDemo;
                cueDemo=c.action==Action::Effect?c.value:-1;demoStart=now;
                gearTravelSeconds=s.gearDemoSeconds;gearRestSeconds=gearDemoRestSeconds(s);
                if(c.action==Action::FlightDemo){timeline=flightDemoStages(s);view.timeline=timeline;}
                connect(s);view.message="Waiting for the headset before starting the test.";break;
            }
            // Real DCS status keeps updating during tests, without mixing sources.
            TelemetryPacket packet;
            if(reader.read(packet) && (packet.sequence!=lastSequence || packet.session!=lastSession)){
                Frame f;std::string error;
                if(parseFrame(packet.payload,f,error)){
                    f.simTime=packet.simTime;f.session=packet.session;f.sequence=packet.sequence;f.receivedMs=now;
                    flightFeed.observe(f,packet.publishedMs);
                    if(!synthetic){lastFrame=f;if(!labMode&&!rawWait&&!rawEnd)engine.ingest(f,now,s.staleMs);}
                    lastSession=packet.session;lastSequence=packet.sequence;
                }else{view.message="Telemetry rejected: "+error;flightFeed.reset();if(!synthetic)engine.reset();}
            }
            if(synthetic){
                if(demoWaiting && haptics.status=="Headset connected" && !haptics.faulted){demoWaiting=false;demoStart=now;view.message="Test playing.";}
                const auto elapsed=double(now-demoStart)/1000.;
                const double duration=gearDemo?gearDemoDuration(gearTravelSeconds,gearRestSeconds):cueDemo>=0?
                    effectDemoDuration(size_t(cueDemo),s):timeline.back().start+timeline.back().duration;
                if(demoWaiting){if(elapsed>10 || haptics.faulted){endTest();view.message="Test cancelled: headset did not become ready.";}}
                else if(elapsed>=duration){
                    const bool completedFlight=!gearDemo && cueDemo<0;
                    endTest();view.demoComplete=completedFlight;view.demoSeconds=completedFlight?duration:0;view.message="Demo complete.";
                }else{
                    lastFrame=gearDemo?gearDemoFrame(elapsed,now,gearTravelSeconds,gearRestSeconds):cueDemo>=0?
                        effectDemoFrame(size_t(cueDemo),elapsed,now):flightDemoFrame(elapsed,now,timeline,gearTravelSeconds);
                    engine.ingest(lastFrame,now);view.demoSeconds=elapsed;
                }
            }
            const auto flight=flightFeed.status(now,s.staleMs);
            const bool fresh=flight.supported,live=!s.muted && fresh && s.activeAircraft==aircraftProfile(flight.aircraft)->id && !synthetic && !labMode && !rawWait && !rawEnd;
            if(live || synthetic)connect(s);
            if(rawWait && haptics.status=="Headset connected" && !haptics.faulted){
                rawWait=0;rawStart=now;rawEnd=now+2750;view.message="Test playing; a stop command follows automatically.";
            }
            if(rawWait && now>rawWait){endTest();view.message="Test cancelled: headset did not become ready.";}
            if(rawEnd && now>=rawEnd){endTest();view.message="Demo complete.";}
            const bool rawPlaying=rawEnd && now<rawEnd;
            if(labActive>=0){
                if(s.muted || haptics.faulted){endTest();view.message="Test stopped.";}
                else if(labWait && haptics.status=="Headset connected"){
                    labWait=0;labStart=now;view.message="";
                }else if(labWait && now>=labWait){endTest();view.message="Test cancelled: headset did not become ready.";}
                if(labActive>=0 && !labWait && now-labStart>=hapticTestDurationMs){endTest();view.message="Test complete.";}
            }
            auto mixSettings=s;
            if(synthetic && (gearDemo || cueDemo>=0))for(size_t i=0;i<effectCount;++i)mixSettings.effects[i].enabled=int(i)==(gearDemo?int(Gear):cueDemo);
            if(synthetic)for(size_t i=0;i<effectCount;++i)mixSettings.effects[i].enabled=mixSettings.effects[i].enabled&&effectSupported(i,profileById(s.activeAircraft));
            auto mix=engine.tick(now,mixSettings);
            const int patternedMotor=rawMotor && now>=rawStart && (now-rawStart)%750<500?rawMotor:0;
            int motor=rawPlaying?patternedMotor:(live || (synthetic&&!demoWaiting)?mix.motor:0);
            if(labMode)motor=labActive>=0&&!labWait?hapticTestMotor(labTest,int(now-labStart),s.motorCap):0;
            if(s.muted)motor=0;
            haptics.update(motor,now);
            if(view.message=="Connecting; SteamVR must be running." && haptics.status=="Headset connected")view.message="Headset ready. DCS effects follow your flight automatically.";
            if(!live && !synthetic && !rawPlaying && !rawWait && labActive<0){if(!idleSince)idleSince=now;
                if(!manualConnection && now-idleSince>3000 && haptics.running())haptics.stop();
            }else idleSince=0;
            view.mixRouted=!s.muted && !haptics.faulted && haptics.status=="Headset connected" &&
                !labMode && !rawPlaying && !rawWait && (live || (synthetic&&!demoWaiting));
            view.mixAircraft=s.activeAircraft;
            view.mix=mix;view.frame=lastFrame;view.requested=motor;view.acknowledged=haptics.acknowledgedMotor;
            view.headset=haptics.status;view.fault=haptics.faulted;view.fresh=fresh;view.flight=flight;view.ageMs=flight.ageMs;
            view.source=synthetic?(gearDemo?"Gear up/down demo":cueDemo>=0?std::string(effectNames[cueDemo])+" audition":"Demo flight"):rawPlaying||rawWait?"Headset test":reader.status;
            view.labMode=labMode;view.labActive=labActive;view.labElapsedMs=labActive>=0&&!labWait?int(now-labStart):0;
            if(labMode)view.source=labActive>=0?hapticPatternNames[size_t(labActive)]:"Haptic tests";
            view.testing=synthetic||rawPlaying||rawWait||labActive>=0;view.flightDemo=synthetic&&!gearDemo&&cueDemo<0;view.demoWaiting=demoWaiting||rawWait||labWait;
        }catch(const std::exception& e){endTest();view.message=e.what();view.requested=0;view.mixRouted=false;view.mix={};view.testing=view.flightDemo=view.demoWaiting=false;}
        {std::lock_guard lock(mutex_);snapshot_=view;}
        std::this_thread::sleep_for(std::chrono::milliseconds(labActive>=0?10:20));
    }
    haptics.update(0,GetTickCount64());haptics.stop();
}

void App::render(void* logo,std::function<void*(const std::string&)> socialIcon){
    auto s=settings();const auto v=snapshot();bool changed=false;
    if(v.flight.supported)changed=s.selectAircraft(aircraftProfile(v.flight.aircraft)->id);
    const auto now=GetTickCount64();
    if(!hookCheckAt_ || now-hookCheckAt_>=2000){
        hookCheckAt_=now;installedHooks_=0;hookDetail_.clear();auto knownProfiles=profiles_;
        for(const auto& path:s.dcsProfiles){auto p=fs::path(wide(path));if(std::find(knownProfiles.begin(),knownProfiles.end(),p)==knownProfiles.end())knownProfiles.push_back(p);}
        hookProfiles_=knownProfiles.size();
        for(const auto& p:knownProfiles){const bool installed=integrationInstalled(p);installedHooks_+=installed?1:0;
            hookDetail_+=utf8(p.filename().wstring())+(installed?": installed\n":": not installed\n");}
        hookDetail_+="Checks the export entry and its Lua/DLL files. After installing or repairing the hook, restart DCS.";
    }
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("dcs2psvr2-shaker",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    if(ImGui::BeginTable("AppHeader",3)){
        ImGui::TableSetupColumn("Brand",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Pages",ImGuiTableColumnFlags_WidthFixed,400);
        ImGui::TableSetupColumn("Stop",ImGuiTableColumnFlags_WidthFixed,112);
        ImGui::TableNextColumn();ImGui::BeginGroup();
        if(logo){ImGui::Image(ImTextureID(reinterpret_cast<uintptr_t>(logo)),{64,64});ImGui::SameLine(0,14);}
        ImGui::BeginGroup();
        auto& fonts=ImGui::GetIO().Fonts->Fonts;ImGui::PushFont(fonts.Size>1?fonts[1]:nullptr,34);
        const float titleBaseline=ImGui::GetCursorPosY()+ImGui::GetFontBaked()->Ascent;
        ImGui::TextColored({.35f,.73f,.84f,1},"dcs2psvr2-shaker");ImGui::PopFont();
        ImGui::SameLine(0,10);ImGui::PushFont(nullptr,16);
        ImGui::SetCursorPosY(titleBaseline-ImGui::GetFontBaked()->Ascent);ImGui::TextColored({.35f,.73f,.84f,1},"(GitHub)");ImGui::PopFont();
        ImGui::PushFont(nullptr,20);ImGui::TextUnformatted("by Adam Chesters");ImGui::PopFont();ImGui::EndGroup();ImGui::EndGroup();
        if(ImGui::IsItemHovered()){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);ImGui::SetTooltip("Open dcs2psvr2-shaker on GitHub");}
        if(ImGui::IsItemClicked())ShellExecuteW(nullptr,L"open",L"https://github.com/AdamChesters/dcs2psvr2-shaker",nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::TableNextColumn();const char* pages[]={"Effects","Headset","Settings","Tests"};
        for(int i=0;i<4;++i){
            if(i)ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,page_==i?ImVec4(.14f,.29f,.34f,1):ImVec4(.055f,.07f,.08f,1));
            if(ImGui::Button(pages[i],{92,38}) && page_!=i){
                if(page_==3)command({Action::LabExit});
                page_=i;if(page_==3)command({Action::LabEnter});
            }ImGui::PopStyleColor();
        }
        ImGui::TableNextColumn();ImGui::PushStyleColor(ImGuiCol_Button,{.29f,.12f,.13f,1});
        if(ImGui::Button("STOP",{108,38})){emergencyStop();s.muted=true;changed=true;}ImGui::PopStyleColor();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Stop and mute all output. Ctrl + Alt + Space.");ImGui::EndTable();
    }
    ImGui::Spacing();
    if(ImGui::Button("Setup"))showSetup_=true;
    continueRow("Feedback / Support");
    if(ImGui::Button("Feedback / Support",{0,44}))showSupport_=true;
    renderSetup();
    const float statusWidth=ImGui::GetContentRegionAvail().x;
    const int statusColumns=statusWidth>=980.f?4:statusWidth>=640.f?2:1;
    if(ImGui::BeginTable("ConnectionLights",statusColumns,ImGuiTableFlags_SizingStretchSame,{std::min(statusWidth,980.f),0})){
        ImGui::TableNextColumn();renderUpdates();
        const auto presenceLight=[&](const char* label,Presence state,const char* positive,const char* help,const char* missing){
            ImGui::TableNextColumn();
            statusLight(label,state==Presence::Present?positive:state==Presence::Absent?"Not detected":"Unknown",state==Presence::Present,help);
            if(state!=Presence::Present)mutedText(state==Presence::Unknown?"Status check unavailable; check manually.":missing);
        };
        presenceLight("Headset",v.system.headset,"USB connected","PSVR2 is present in Windows USB devices. This does not verify tracking, power state or rumble unlock.","Connect and power on the headset.");
        ImGui::TableNextColumn();
        statusLight("Jailbreak","Unverified",false,"The Toolkit API has no jailbreak status query. Command acceptance does not validate physical rumble.");
        mutedText("Run jailbreak before SteamVR.");
        presenceLight("SteamVR",v.system.steamVR,"Running","Detects vrserver.exe. Headset tracking and Toolkit readiness are separate.","Run jailbreak first, then start SteamVR.");
        presenceLight("DCS",v.system.dcs,"Running","Detects DCS.exe, including menus or paused flight. A dedicated DCS server is not the flight client.","Start DCS and enter a flight for effects.");
        ImGui::TableNextColumn();const auto hook=installedHooks_?(installedHooks_==hookProfiles_?std::string("Installed"):std::to_string(installedHooks_)+"/"+std::to_string(hookProfiles_)+" profiles"):"Not installed";
        statusLight("DCS hook",hook,installedHooks_>0,hookDetail_);
        if(!installedHooks_)mutedText("Install the export hook in Settings.");
        ImGui::TableNextColumn();statusLight("DCS telemetry",v.flight.telemetryLive?"Live":"Waiting / paused",v.flight.telemetryLive,"Lights when real shared-memory telemetry has an advancing DCS clock. Tests do not change this light.");
        if(!v.flight.telemetryLive)mutedText("Enter or resume a flight.");
        ImGui::TableNextColumn();const auto aircraft=v.flight.aircraftLive?(v.flight.supported?std::string("Live / ")+aircraftProfile(v.flight.aircraft)->name:std::string("Live / unsupported")):"Waiting";
        statusLight("Aircraft",aircraft,v.flight.aircraftLive,"Requires aircraft identity and numeric signals in the live DCS feed. Individual effects still depend on their own signals. Aircraft: "+(v.flight.aircraft.empty()?std::string("none"):v.flight.aircraft));
        if(!v.flight.aircraftLive)mutedText("Enter an aircraft to receive flight signals.");
        ImGui::EndTable();
    }
    ImGui::Separator();ImGui::Spacing();
    const auto message=!uiMessage_.empty()?uiMessage_:v.message;
    const std::string status=v.testing?v.source+(v.demoWaiting?" / waiting for headset":" / playing"):
        v.fault?"Headset output fault. Reconnect in Headset.":s.muted?"Output muted":v.headset;
    const float footerWidth=std::max(100.f,ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ScrollbarSize);
    const float statusHeight=ImGui::CalcTextSize(status.c_str(),nullptr,false,footerWidth-(v.testing?100.f:0.f)).y;
    const float messageHeight=message.empty()?0:ImGui::CalcTextSize(message.c_str(),nullptr,false,footerWidth).y+ImGui::GetStyle().ItemSpacing.y;
    const float footerHeight=std::clamp(statusHeight+messageHeight+24.f,52.f,140.f);
    const auto pageId=std::string("Page")+std::to_string(page_);
    ImGui::BeginChild(pageId.c_str(),{0,-footerHeight},ImGuiChildFlags_None);ImGui::PushItemWidth(-260);
    auto audition=[&](size_t i){s.muted=false;changed=true;command({i==Gear?Action::GearDemo:Action::Effect,int(i)});};
    auto drawEffect=[&](size_t i){
        auto& e=s.effects[i];const auto& definition=effectDefinition(i);ImGui::PushID(int(i));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{.042f,.052f,.06f,1});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,10});
        ImGui::BeginChild("Cue",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY);
        if(ImGui::BeginTable("Controls",6,ImGuiTableFlags_SizingStretchProp)){
            ImGui::TableSetupColumn("Enabled",ImGuiTableColumnFlags_WidthFixed,28);
            ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Activity",ImGuiTableColumnFlags_WidthFixed,92);
            ImGui::TableSetupColumn("Strength",ImGuiTableColumnFlags_WidthFixed,210);
            ImGui::TableSetupColumn("Test",ImGuiTableColumnFlags_WidthFixed,68);
            ImGui::TableSetupColumn("Tune",ImGuiTableColumnFlags_WidthFixed,76);
            ImGui::TableNextColumn();changed|=ImGui::Checkbox("##Live",&e.enabled);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Enable %s during live flight",effectNames[i]);
            ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextWrapped("%s",effectNames[i]);
            ImGui::TableNextColumn();
            const bool matching=v.mixAircraft==s.activeAircraft && (e.enabled||v.testing);
            activityLight(matching?effectActivity(v.mix,i,v.mixRouted&&!s.muted):EffectActivity::Idle,
                !e.enabled&&!v.testing?"Disabled":!matching?"Waiting for this aircraft profile":v.mix.effects[i].reason);
            ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
            if(ImGui::SliderInt("##Strength",&e.motorMax,10,25,"Strength %d")){e.motorMin=std::min(e.motorMin,e.motorMax);changed=true;}
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Peak motor command. Your master response and ceiling also apply.");
            ImGui::TableNextColumn();if(ImGui::Button("Test",{64,0}))audition(i);
            ImGui::TableNextColumn();if(ImGui::Button(expanded_[i]?"Less":"Tune",{72,0}))expanded_[i]=!expanded_[i];
            ImGui::EndTable();
        }
        if(expanded_[i]){
            ImGui::Separator();ImGui::TextWrapped("%s",definition.description);
            ImGui::TextWrapped("Test plays this cue alone, even when its live switch is off.");
            ImGui::PushItemWidth(-280);
            if(i==Gear){
                changed|=tuningFloat("Demo travel per direction (s)",&s.gearDemoSeconds,3,15,"%.1f");
                mutedText("Live travel follows DCS gear movement.");
                if(ImGui::CollapsingHeader("Rhythm")){
                    changed|=tuningInt("Start hit (ms)",&s.gearStartMs,50,1000);
                    changed|=tuningInt("Quiet after start (ms)",&s.gearStartGapMs,0,1500);
                    changed|=tuningInt("Travel ramp (ms)",&s.gearRampMs,0,3000);
                    changed|=tuningInt("Quiet before lock (ms)",&s.gearLockGapMs,0,1500);
                    changed|=tuningInt("Lock hit (ms)",&s.gearLockMs,50,1000);
                    if(ImGui::Button("Apply gear preset")){applyGearPreset(s);changed=true;uiMessage_="Gear preset applied, including 4.8-second demo travel per direction.";}
                }
            }else if(ImGui::CollapsingHeader("Timing")){
                if(definition.shape==CueShape::Continuous)changed|=tuningInt("Stop fade (ms)",&e.settleMs,0,1000);
                else{
                    changed|=tuningInt(definition.shape==CueShape::Burst?"Burst hold (ms)":"Hit length (ms)",&e.holdMs,80,1000);
                    changed|=tuningInt(definition.shape==CueShape::Surge?"Rumble tail (ms)":"Settle tail (ms)",&e.settleMs,0,1000);
                    changed|=tuningInt("Retrigger recovery (ms)",&e.coastMs,0,1500);
                    if(definition.shape!=CueShape::Burst)changed|=tuningInt("Repeat cooldown (ms)",&e.cooldownMs,0,3000);
                }
            }
            if(ImGui::CollapsingHeader("Response and mixing")){
                changed|=tuningInt("Minimum strength",&e.motorMin,10,e.motorMax);
                changed|=tuningFloat("Response gain",&e.gain,0,2,"%.2f");
                changed|=tuningFloat("Threshold",&e.threshold,0,.9f,"%.3f");
                changed|=tuningFloat("Curve",&e.curve,.2f,3,"%.2f");
                changed|=tuningFloat("Attack (ms)",&e.attackMs,0,500,"%.0f");
                changed|=tuningFloat("Release (ms)",&e.releaseMs,20,1000,"%.0f");
                changed|=tuningInt("Priority",&e.priority,0,100);
                ImGui::TextWrapped("Higher-priority cues take over. Background resumes when an event finishes. Gear gaps remain quiet.");
            }
            if(ImGui::CollapsingHeader("Command preview and activity")){
                static std::array<std::string,effectCount> keys;
                static std::array<std::array<float,400>,effectCount> previews{};
                const auto key=s.json().dump();auto& preview=previews[i];
                if(key!=keys[i]){
                    keys[i]=key;auto previewSettings=s;
                    for(size_t j=0;j<effectCount;++j)previewSettings.effects[j].enabled=j==i;
                    EffectEngine engine;const double duration=effectDemoDuration(i,s);
                    for(size_t j=0;j<preview.size();++j){
                        const double t=duration*j/(preview.size()-1);const auto now=1000+uint64_t(t*1000);
                        engine.ingest(i==Gear?gearDemoFrame(t,now,s.gearDemoSeconds,gearDemoRestSeconds(s)):effectDemoFrame(i,t,now),now);
                        preview[j]=float(engine.tick(now,previewSettings).motor);
                    }
                }
                ImGui::PlotLines("##Preview",preview.data(),int(preview.size()),0,"Demo timeline / zero = coast",0,25,{-1,90});
                mutedText("Motor commands, not measured physical force.");
                ImGui::TextWrapped("Current activity: %s",v.mix.effects[i].reason.c_str());
            }
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();ImGui::PopStyleVar();ImGui::PopStyleColor();ImGui::PopID();
    };
    if(page_==0){
        heading("Feel your flight.","Your headset mix. Effects follow your flight automatically.");
        if(ImGui::BeginTable("FlightControls",3)){
            ImGui::TableSetupColumn("Flight status",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Master",ImGuiTableColumnFlags_WidthFixed,230);
            ImGui::TableSetupColumn("Mute",ImGuiTableColumnFlags_WidthFixed,110);
            ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();mutedText(s.muted?"Output muted":v.fresh?"Aircraft connected":"Waiting for a supported DCS aircraft");
            ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);float percent=s.master*100;
            if(ImGui::SliderFloat("##Master",&percent,0,100,"Master %.0f%%")){s.master=percent/100;changed=true;}
            ImGui::TableNextColumn();changed|=ImGui::Checkbox("Muted",&s.muted);ImGui::EndTable();
        }
        ImGui::Text("Profile: %s",profileById(s.activeAircraft)->name);
        mutedText("Green: active layer. Ring: output priority. Gap: planned silence.");
        mutedText("Strength and graphs show motor commands, not measured vibration.");
        ImGui::Spacing();
        static const auto alphabetical=[] {
            std::array<size_t,effectCount> order{};
            for(size_t i=0;i<effectCount;++i)order[i]=i;
            std::sort(order.begin(),order.end(),[](size_t a,size_t b){return std::string_view(effectNames[a])<std::string_view(effectNames[b]);});
            return order;
        }();
        for(const auto i:alphabetical)if(effectSupported(i,profileById(s.activeAircraft)))drawEffect(i);
    }else if(page_==1){
        heading("Find your feel.","Connect the headset and choose a comfortable strength.");
        ImGui::TextWrapped("%s",v.headset.c_str());
        if(ImGui::Button("Connect / reconnect"))command({Action::Connect});continueRow("Start SteamVR");
        if(ImGui::Button("Start SteamVR"))ShellExecuteW(nullptr,L"open",L"steam://rungameid/250820",nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::Dummy({0,12});
        ImGui::SeparatorText("Test the headset");
        tuningInt("Test strength",&testMotor_,10,25);
        if(ImGui::Button("Test headset",{180,38})){s.muted=false;changed=true;command({Action::Raw,testMotor_});}
        continueRow("BRR  BRR  BRR  BRR");mutedText("BRR  BRR  BRR  BRR");
        ImGui::TextWrapped("Four 500 ms vibrations with quiet gaps. This direct test uses the selected strength, independently of your flight mix and ceiling.");
        ImGui::Spacing();
        if(ImGui::Button(v.flightDemo?"Restart demo flight":"Demo flight",{180,38})){s.muted=false;changed=true;command({Action::FlightDemo});}
        continueRow("Flight timeline");
        if(ImGui::Button("Flight timeline"))showTimeline_=!showTimeline_;
        mutedText("Plays your enabled mix through the headset, including your saved gear travel time. Master and flight ceiling apply.");
        if(showTimeline_ || v.flightDemo){
            const auto stages=(v.flightDemo||v.demoComplete)&&!v.timeline.empty()?v.timeline:flightDemoStages(s);
            const double duration=stages.back().start+stages.back().duration;
            const double elapsed=v.flightDemo||v.demoComplete?v.demoSeconds:0;
            const int active=v.flightDemo&&!v.demoWaiting?flightDemoStageAt(stages,elapsed):-1;
            char progress[96];snprintf(progress,sizeof(progress),"%s / %.1f of %.1f s",v.demoWaiting&&v.flightDemo?"Waiting for headset":v.flightDemo?"Playing":v.demoComplete?"Complete":"Ready",elapsed,duration);
            ImGui::ProgressBar(float(elapsed/duration),{-1,22},progress);
            if(ImGui::BeginTable("FlightTimeline",2,ImGuiTableFlags_SizingStretchProp)){
                ImGui::TableSetupColumn("Time",ImGuiTableColumnFlags_WidthFixed,124);
                ImGui::TableSetupColumn("Stage",ImGuiTableColumnFlags_WidthStretch);
                for(size_t i=0;i<stages.size();++i){const auto& stage=stages[i];const bool current=int(i)==active;
                    ImGui::TableNextRow();
                    if(current)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImVec4(.10f,.25f,.30f,1)));
                    ImGui::TableNextColumn();ImGui::Text("%s %04.1f-%04.1f s",current?">":elapsed>=stage.start+stage.duration?"+":" ",stage.start,stage.start+stage.duration);
                    ImGui::TableNextColumn();ImGui::TextWrapped("%s",stage.title);
                    if(current)ImGui::TextWrapped("%s",stage.description);
                    else if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::PushTextWrapPos(420);ImGui::TextUnformatted(stage.description);ImGui::PopTextWrapPos();ImGui::EndTooltip();}
                }ImGui::EndTable();
            }
            mutedText("Highlighted = current stage; + = finished. Disabled cues stay quiet. This is a test sequence, not DCS telemetry.");
        }
        ImGui::Dummy({0,12});
        if(tuningInt("Flight strength ceiling",&s.motorCap,10,25)){s.motorFloor=std::min(s.motorFloor,s.motorCap);changed=true;}
        mutedText("Every effect and effect test respects this ceiling.");
        ImGui::Dummy({0,10});
        if(ImGui::CollapsingHeader("Fine-tune your useful range")){
            tuningInt("Lowest clearly felt",&calibrationLow_,10,25);calibrationHigh_=std::max(calibrationLow_,calibrationHigh_);
            tuningInt("Comfortable ceiling",&calibrationHigh_,calibrationLow_,25);
            if(ImGui::Button("Fit all cues to this range")){fitEffectRanges(s,calibrationLow_,calibrationHigh_);changed=true;uiMessage_="Cue ranges fitted to your chosen motor range.";}
            changed|=tuningFloat("Motor response curve",&s.motorCurve,.25f,3,"%.2f");
            ImGui::TextWrapped("1.00 spaces command settings evenly. Lower values strengthen medium cues; higher values soften them. This is a subjective response curve.");
        }
        if(ImGui::CollapsingHeader("Connection help")){
            ImGui::TextWrapped("Run the jailbreak before SteamVR after each headset power-on.");
            if(ImGui::Button("Open setup checklist"))showSetup_=true;
            ImGui::TextWrapped("If output faults, recover the VR runtime and reconnect. A toolkit acknowledgement does not prove physical vibration.");
        }
    }else if(page_==3){
        heading("Haptic tests","Compare timing and feel. Every graph spans 6 seconds, with commands from 0 to 25.");
        mutedText("20 ms steps. Direct motor commands; ceiling applies. DCS output is paused on this page.");
        ImGui::BeginDisabled(v.labActive>=0);
        if(tuningInt("Test ceiling",&s.motorCap,10,25)){s.motorFloor=std::min(s.motorFloor,s.motorCap);changed=true;}
        ImGui::EndDisabled();
        if(ImGui::Button("Connect / reconnect"))command({Action::Connect});
        continueRow("Stop test");if(ImGui::Button("Stop test"))command({Action::EndTest});
        ImGui::SameLine();ImGui::Text("Requested %d / acknowledged %d",v.requested,v.acknowledged);
        ImGui::Spacing();
        if(ImGui::BeginTable("HapticPatterns",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH)){
            ImGui::TableSetupColumn("Test",ImGuiTableColumnFlags_WidthFixed,300);
            ImGui::TableSetupColumn("Command",ImGuiTableColumnFlags_WidthStretch);
            for(size_t i=0;i<labTests_.size();++i){
                ImGui::PushID(int(i));auto& test=labTests_[i];const bool playing=v.labActive==int(i);
                ImGui::TableNextRow();ImGui::TableNextColumn();
                if(ImGui::Button(hapticPatternNames[i],{-1,32})){
                    s.muted=false;changed=true;command({Action::LabPlay,int(i),test});
                }
                ImGui::BeginDisabled(playing);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6,3});
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,4});
                tuningInt("Strength",&test.strength,10,25);
                if(test.pattern==HapticPattern::Sweep || test.pattern==HapticPattern::Layered)
                    tuningInt(test.pattern==HapticPattern::Sweep?"Start strength":"Background",&test.low,10,test.strength);
                tuningInt(test.pattern==HapticPattern::Sweep?"Sweep (ms)":"Length (ms)",&test.durationMs,20,
                    test.pattern==HapticPattern::Steady||test.pattern==HapticPattern::Sweep?4000:1000);
                if(test.pattern==HapticPattern::DoubleKnock || test.pattern==HapticPattern::Rhythm)
                    tuningInt("Gap (ms)",&test.gapMs,0,1500);
                test=boundedHapticTest(test);
                ImGui::PopStyleVar(2);ImGui::EndDisabled();
                ImGui::TableNextColumn();
                const auto origin=ImGui::GetCursorScreenPos();
                const float width=std::max(80.f,ImGui::GetContentRegionAvail().x),height=108;
                ImGui::InvisibleButton("##Graph",{width,height+24});
                auto* draw=ImGui::GetWindowDrawList();
                draw->AddRectFilled(origin,{origin.x+width,origin.y+height},ImGui::GetColorU32(ImGuiCol_FrameBg),4);
                const float left=origin.x+22,right=origin.x+width-10,top=origin.y+6,bottom=origin.y+height-6;
                const auto grid=ImGui::GetColorU32(ImGuiCol_Border),ink=ImGui::GetColorU32(ImGuiCol_TextDisabled);
                draw->AddText({origin.x+2,top},ink,"25");draw->AddText({origin.x+7,bottom-ImGui::GetFontSize()},ink,"0");
                for(int sec=0;sec<=6;++sec){
                    const float x=left+(right-left)*sec/6;
                    draw->AddLine({x,top},{x,bottom},grid);
                    char label[8];std::snprintf(label,sizeof(label),"%ds",sec);
                    draw->AddText({x-ImGui::CalcTextSize(label).x*.5f,origin.y+height+3},ink,label);
                }
                for(int value:{0,10,25}){
                    const float y=bottom-(bottom-top)*value/25;
                    draw->AddLine({left,y},{right,y},grid);
                }
                auto point=[&](int ms,int motor){return ImVec2(left+(right-left)*ms/hapticTestDurationMs,bottom-(bottom-top)*motor/25);};
                int prior=hapticTestMotor(test,0,s.motorCap);
                for(int ms=20;ms<=hapticTestDurationMs;ms+=20){
                    const int value=hapticTestMotor(test,ms,s.motorCap);
                    draw->AddLine(point(ms-20,prior),point(ms,prior),ImGui::GetColorU32(ImGuiCol_PlotLines),2);
                    if(value!=prior)draw->AddLine(point(ms,prior),point(ms,value),ImGui::GetColorU32(ImGuiCol_PlotLines),2);
                    prior=value;
                }
                if(playing&&!v.demoWaiting){
                    const float x=point(v.labElapsedMs,0).x;
                    draw->AddLine({x,top},{x,bottom},ImGui::GetColorU32(ImGuiCol_Text),2);
                }
                if(ImGui::IsItemHovered()){
                    const int ms=std::clamp(int((ImGui::GetIO().MousePos.x-left)/(right-left)*6000),0,6000)/20*20;
                    ImGui::SetTooltip("%.2f s / command %d",ms/1000.f,hapticTestMotor(test,ms,s.motorCap));
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        mutedText("Graphs show requested commands, not measured vibration. Settings last for this session.");
    }else{
        heading("Make it yours.","Profiles, DCS integration and the occasional deeper adjustment.");
        if(ImGui::CollapsingHeader("Profiles and presets")){
            ImGui::TextWrapped("Aircraft tuning: %s. Live DCS aircraft select their saved tuning automatically.",profileById(s.activeAircraft)->name);
            ImGui::BeginDisabled(v.flight.supported);
            if(ImGui::BeginCombo("Edit aircraft profile",profileById(s.activeAircraft)->name)){
                for(const auto& aircraft:aircraftProfiles())
                    if(ImGui::Selectable(aircraft.name,s.activeAircraft==aircraft.id))changed=s.selectAircraft(aircraft.id)||changed;
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            char name[128]{};strncpy_s(name,s.profile.c_str(),_TRUNCATE);
            if(labeledControl("Profile name",[&]{return ImGui::InputText("##Name",name,sizeof(name));})){s.profile=name;changed=true;}
            if(ImGui::Button("Apply default headset mix")){applyHeadsetMix(s);changed=true;uiMessage_="Flight cues updated. Your gear rhythm, demo travel and headset range were preserved.";}
            ImGui::TextWrapped("Aircraft-appropriate flight cues on; engine ambience and runway bumps off. Preserves gear tuning, master response and the headset ceiling.");
            if(ImGui::Button("Export profile...")){auto p=chooseFile(true,jsonFilter,L"json");if(!p.empty())try{
                auto j=s.json();for(const char* k:{"toolkitPath","dcsProfiles","muted","aircraftTuning"})j.erase(k);
                writeTextAtomic(p,j.dump(2));uiMessage_="Profile exported.";
            }catch(const std::exception& e){uiMessage_=e.what();}}
            continueRow("Import profile...");if(ImGui::Button("Import profile...")){auto p=chooseFile(false,jsonFilter,L"json");if(!p.empty())try{
                auto imported=Settings::fromJson(Json::parse(readText(p)));fitEffectRanges(imported,s.motorFloor,s.motorCap);
                s.effects=imported.effects;s.master=imported.master;s.gearStartMs=imported.gearStartMs;s.gearLockMs=imported.gearLockMs;
                s.gearStartGapMs=imported.gearStartGapMs;s.gearLockGapMs=imported.gearLockGapMs;s.gearRampMs=imported.gearRampMs;
                s.gearDemoSeconds=imported.gearDemoSeconds;s.profile=imported.profile;changed=true;uiMessage_="Profile imported and fitted to your local motor range.";
            }catch(const std::exception& e){uiMessage_=e.what();}}
            if(ImGui::TreeNode("More presets")){
                auto defaults=[&]{auto mix=Settings{};mix.selectAircraft(s.activeAircraft);fitEffectRanges(mix,s.motorFloor,s.motorCap);s.effects=mix.effects;};
                if(ImGui::Button("Comfort")){defaults();s.master=.65f;s.profile=std::string(profileById(s.activeAircraft)->name)+" - Comfort";changed=true;}
                continueRow("Events only");if(ImGui::Button("Events only")){defaults();for(size_t i:{size_t(Buffet),size_t(Airflow),size_t(AfterburnerRumble),size_t(Engine),size_t(Taxi)})s.effects[i].enabled=false;s.profile=std::string(profileById(s.activeAircraft)->name)+" - Events";changed=true;}
                ImGui::TreePop();
            }
        }
        if(ImGui::CollapsingHeader("DCS integration")){
            if(profiles_.empty())ImGui::TextWrapped("No DCS Saved Games profiles found. Launch DCS once, then refresh.");
            for(const auto& p:profiles_){const auto path=utf8(p.wstring());ImGui::PushID(path.c_str());ImGui::TextWrapped("%s",path.c_str());
                const bool installed=integrationInstalled(p);mutedText(installed?"Export installed":"Export not installed");
                if(ImGui::Button(installed?"Repair export":"Install export"))try{
                    installIntegration(p,appDirectory());if(std::find(s.dcsProfiles.begin(),s.dcsProfiles.end(),path)==s.dcsProfiles.end())s.dcsProfiles.push_back(path);
                    changed=true;uiMessage_="Export installed. Restart DCS to load it.";
                }catch(const std::exception& e){uiMessage_=e.what();}
                if(installed){continueRow("Remove our export");if(ImGui::Button("Remove our export"))try{removeIntegration(p);std::erase(s.dcsProfiles,path);changed=true;uiMessage_="Our export removed.";}catch(const std::exception& e){uiMessage_=e.what();}}
                ImGui::PopID();ImGui::Spacing();
            }
            if(ImGui::Button("Refresh DCS profiles"))try{profiles_=dcsProfiles();}catch(const std::exception& e){uiMessage_=e.what();}
        }
        if(ImGui::CollapsingHeader("Advanced connection and diagnostics")){
            auto capi=s.toolkitPath.empty()?toolkitFile():fs::path(wide(s.toolkitPath));
            ImGui::TextWrapped("%s",capi.empty()?"Toolkit CAPI DLL not found":utf8(capi.wstring()).c_str());
            if(ImGui::Button("Select toolkit DLL...")){auto p=chooseFile(false,L"PSVR2Toolkit CAPI\0psvr2_toolkit_capi.dll\0\0",L"dll");if(!p.empty()){s.toolkitPath=utf8(p.wstring());changed=true;}}
            continueRow("Detect automatically");if(ImGui::Button("Detect automatically")){s.toolkitPath.clear();changed=true;}
            changed|=tuningInt("Stale cutoff (ms)",&s.staleMs,150,1000);
            ImGui::Text("Motor requested %d / acknowledged %d",v.requested,v.acknowledged);
            ImGui::TextWrapped("Telemetry: %s",v.source.c_str());
            if(ImGui::Button("Export diagnostic report...")){auto p=chooseFile(true,jsonFilter,L"json");if(!p.empty())try{
                Json j={{"appVersion",appVersion},{"headset",v.headset},{"source",v.source},{"requested",v.requested},{"acknowledged",v.acknowledged},{"telemetry",frameJson(v.frame)},{"ageMs",v.ageMs},{"toolkitSha256",capi.empty()?"missing":sha256(capi)}};
                writeTextAtomic(p,j.dump(2));uiMessage_="Diagnostic report saved; review before sharing.";
            }catch(const std::exception& e){uiMessage_=e.what();}}
            if(ImGui::TreeNode("Live signal values")){
                if(ImGui::BeginTable("Signals",2,ImGuiTableFlags_RowBg)){
                    for(const auto& [key,value]:v.frame.values){ImGui::TableNextColumn();ImGui::TextUnformatted(key.c_str());ImGui::TableNextColumn();ImGui::Text("%.5g",value);}ImGui::EndTable();
                }ImGui::TreePop();
            }
            ImGui::TextWrapped("Damage and ejection are deferred until reliable own-aircraft events are verified. Missing signals cannot trigger an effect.");
        }
        ImGui::Dummy({0,18});mutedText((std::string("dcs2psvr2-shaker ")+appVersionDisplay).c_str());
        if(ImGui::Button("Source and credits"))ShellExecuteW(nullptr,L"open",L"https://github.com/AdamChesters/dcs2psvr2-shaker",nullptr,nullptr,SW_SHOWNORMAL);
        continueRow("Settings folder");if(ImGui::Button("Settings folder"))openPath(dataDirectory());
        continueRow("Exit application");if(ImGui::Button("Exit application"))PostQuitMessage(0);
        mutedText("Closing the window leaves the app in the tray. Settings save automatically.");
    }
    ImGui::PopItemWidth();ImGui::EndChild();ImGui::Separator();
    ImGui::BeginChild("StatusFooter",{0,0},ImGuiChildFlags_None);
    if(v.testing && ImGui::BeginTable("TestStatus",2)){
        ImGui::TableSetupColumn("Status",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Stop",ImGuiTableColumnFlags_WidthFixed,90);
        ImGui::TableNextColumn();ImGui::TextWrapped("%s",status.c_str());
        ImGui::TableNextColumn();if(ImGui::SmallButton("Stop test"))command({Action::EndTest});ImGui::EndTable();
    }else{
        ImGui::PushStyleColor(ImGuiCol_Text,v.fault?ImVec4(1,.48f,.43f,1):ImVec4(.55f,.67f,.70f,1));
        ImGui::TextWrapped("%s",status.c_str());ImGui::PopStyleColor();
    }
    if(!message.empty())mutedText(message.c_str());
    ImGui::EndChild();
    renderChangelog();
    renderSupport(SupportIdentity{"dcs2psvr2-shaker",logo,appVersion,"dcs2psvr2-shaker",[this]{updates_.check();},std::move(socialIcon)});
    ImGui::End();
    if(changed){std::lock_guard lock(mutex_);settings_=s;settingsDirty_=true;saveAt_=GetTickCount64()+400;}
    if(settingsDirty_ && GetTickCount64()>=saveAt_)try{save();}catch(const std::exception& e){uiMessage_=e.what();settingsDirty_=false;}
}

int runProbe(int motor,const fs::path& output){
    HapticsClient client;Json report={{"appVersion",appVersion},{"requestedMotor",motor}};auto path=toolkitFile();
    try{report["toolkitSha256"]=sha256(path);client.start(path);}catch(const std::exception&e){report["error"]=e.what();writeTextAtomic(output,report.dump(2));return 2;}
    const auto start=GetTickCount64();uint64_t began=0,stopSent=0;bool acknowledged=false,stopped=false;
    while(GetTickCount64()-start<10000){
        auto now=GetTickCount64();int target=0;
        if(!began && client.status=="Headset connected")began=now;
        if(began && now-began<250)target=motor;
        if(began && now-began>=250 && !stopSent)stopSent=now;
        client.update(target,now);
        if(motor && client.acknowledgedMotor==motor)acknowledged=true;
        if(stopSent && now-stopSent>100 && client.acknowledgedMotor==0 && client.status=="Headset connected"){stopped=true;break;}
        if(client.faulted)break;
        Sleep(20);
    }
    report["status"]=client.status;report["commandAcknowledged"]=motor?acknowledged:began!=0;report["stopAcknowledged"]=stopped;
    client.update(0,GetTickCount64());client.stop();writeTextAtomic(output,report.dump(2));return stopped && (!motor||acknowledged)?0:1;
}
}
