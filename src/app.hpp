#pragma once
#include "core.hpp"
#include "haptic_tests.hpp"
#include "platform.hpp"
#include "updates.hpp"
#include "feedback.hpp"
#include "support_identity.hpp"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace shaker {
struct Snapshot {
    SystemStatus system;
    Mix mix;
    Frame frame;
    FlightFeedStatus flight;
    std::string source="Waiting for DCS",headset="Headset output idle",message;
    int requested=0,acknowledged=0;
    uint64_t ageMs=0;
    bool fresh=false,testing=false,flightDemo=false,demoWaiting=false,demoComplete=false,fault=false;
    double demoSeconds=0;
    int labActive=-1,labElapsedMs=0;
    bool labMode=false,mixRouted=false;
    std::string mixAircraft;
    std::vector<FlightDemoStage> timeline;
};
enum class Action { Connect,Stop,Raw,Effect,FlightDemo,GearDemo,EndTest,LabEnter,LabExit,LabPlay };
struct Command {Action action;int value=0;HapticTest test{};};
class App {
    mutable std::mutex mutex_;
    Settings settings_;
    Snapshot snapshot_;
    std::vector<Command> commands_;
    std::jthread worker_,statusWorker_;
    SystemStatus system_;
    UpdateClient updates_;
    FeedbackClient feedback_;
    std::array<char,101> feedbackName_{};
    std::array<char,255> feedbackEmail_{};
    std::array<char,4001> feedbackMessage_{};
    std::string feedbackError_;
    bool showSupport_=false,showFeedback_=false,showUpdates_=false,feedbackPending_=false;
    void renderSupport(const SupportIdentity& identity);
    bool updateInstallRequested_=false;
    std::string updateLaunchError_;
    bool showChangelog_=false,changelogNeedsMark_=false;
    std::string changelog_;
    int page_=0,selectedEffect_=0,testMotor_=15,calibrationLow_=10,calibrationHigh_=18;
    std::array<bool,effectCount> expanded_{};
    std::array<HapticTest,6> labTests_=defaultHapticTests();
    bool settingsDirty_=false,showTimeline_=true;
    uint64_t saveAt_=0;
    uint64_t hookCheckAt_=0;
    size_t hookProfiles_=0,installedHooks_=0;
    std::string hookDetail_;
    std::string uiMessage_;
    std::vector<fs::path> profiles_;
    void run(std::stop_token stop);
    void save();
    void renderUpdates();
    void renderChangelog();
    void renderSetup();
    bool showSetup_=false;
public:
    App();
    ~App();
    void command(Command c);
    void emergencyStop();
    void render(void* logo=nullptr,std::function<void*(const std::string&)> socialIcon={});
    Settings settings()const;
    Snapshot snapshot()const;
};
int runWindow(HINSTANCE instance);
int runProbe(int motor,const fs::path& output);
}
