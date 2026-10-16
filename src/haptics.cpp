#include "haptics.hpp"
#include "core.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace shaker {
namespace {
void diagnostic(const std::string& line){
    try{wchar_t testLog[32768]{};
        const auto file=GetEnvironmentVariableW(L"DCS2PSVR2_SHAKER_TEST_LOG",testLog,32768)?fs::path(testLog):dataDirectory()/L"haptics.log";
        auto mode=std::ios::out|std::ios::app;if(fs::exists(file)&&fs::file_size(file)>1024*1024)mode=std::ios::out|std::ios::trunc;
        std::ofstream out(file,mode);out<<GetTickCount64()<<" pid="<<GetCurrentProcessId()<<" "<<line<<'\n';
    }catch(...){}
}
class Toolkit {
    HMODULE module_=nullptr;
    using Init=int(__cdecl*)(); using Deinit=void(__cdecl*)(); using Active=bool(__cdecl*)(); using Rumble=int(__cdecl*)(uint8_t);
    Init init_=nullptr;Deinit deinit_=nullptr;Active active_=nullptr;Rumble rumble_=nullptr;
    bool initialized_=false;
public:
    std::string error;
    bool load(const fs::path& path) {
        if(path.empty()||!fs::is_regular_file(path)){error="PSVR2Toolkit not found. Start SteamVR or select its CAPI DLL.";return false;}
        const auto hash=sha256(path);
        // Exact tested ABI list. Older DLLs export identical symbols with void returns.
        bool known=false;
        try {
            const auto compatibility=Json::parse(readText(appDirectory()/L"compatibility.json"));
            for(const auto& entry:compatibility.at("toolkit"))if(entry.at("sha256").get<std::string>()==hash && entry.at("abi")=="int-results-v1")known=true;
        }catch(...){}
        if(!known){error="This toolkit DLL build has not been validated. See Diagnostics / compatibility.";return false;}
        module_=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module_){error="Toolkit DLL or a dependency could not load (Windows "+std::to_string(GetLastError())+").";return false;}
        init_=reinterpret_cast<Init>(GetProcAddress(module_,"psvr2_toolkit_init"));
        deinit_=reinterpret_cast<Deinit>(GetProcAddress(module_,"psvr2_toolkit_deinit"));
        active_=reinterpret_cast<Active>(GetProcAddress(module_,"psvr2_toolkit_get_driver_active"));
        rumble_=reinterpret_cast<Rumble>(GetProcAddress(module_,"psvr2_toolkit_set_hmd_rumble"));
        if(!init_||!deinit_||!active_||!rumble_){error="Toolkit API exports are incomplete.";return false;}
        int r=init_();if(r!=0){error="Toolkit client initialization failed: "+std::to_string(r);return false;}
        initialized_=true;return true;
    }
    int set(int motor){if(!initialized_)return -10;if(!active_())return -1;return rumble_(uint8_t(motor));}
    bool active()const{return initialized_&&active_();}
    ~Toolkit(){if(initialized_)deinit_();if(module_)FreeLibrary(module_);}
};
}
int runHapticsHost(const std::wstring& name,const fs::path& capi,DWORD parentPid,const wchar_t* ownerName) {
    HANDLE owner=CreateMutexW(nullptr,TRUE,ownerName);
    if(!owner||GetLastError()==ERROR_ALREADY_EXISTS){if(owner)CloseHandle(owner);return 3;}
    HANDLE pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    if(pipe==INVALID_HANDLE_VALUE){ReleaseMutex(owner);CloseHandle(owner);return 4;}
    DWORD mode=PIPE_READMODE_MESSAGE|PIPE_NOWAIT;SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr);
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,parentPid);
    Toolkit toolkit; bool ready=false;HostReply reply;int lastMotor=-1;uint64_t expiry=0;uint32_t lastSeq=0;
    try {ready=toolkit.load(capi);}catch(const std::exception& e){toolkit.error=e.what();}
    if(!ready){reply.result=-10;strncpy_s(reply.detail,toolkit.error.c_str(),_TRUNCATE);DWORD n;WriteFile(pipe,&reply,sizeof(reply),&n,nullptr);}
    bool quit=false;
    int loggedMotor=-99,loggedResult=-99;
    while(!quit) {
        if(parent&&WaitForSingleObject(parent,0)!=WAIT_TIMEOUT)break;
        HostCommand command;DWORD got=0;bool received=false;
        // Drain queued requests; only the newest unexpired target can reach hardware.
        for(int i=0;i<128;++i){HostCommand next;DWORD n=0;
            if(!ReadFile(pipe,&next,sizeof(next),&n,nullptr)){
                DWORD e=GetLastError();if(e==ERROR_BROKEN_PIPE||e==ERROR_PIPE_NOT_CONNECTED)quit=true;break;
            }
            if(n!=sizeof(next)||next.magic!=command.magic||next.sequence<=lastSeq)continue;
            command=next;got=n;received=true;lastSeq=next.sequence;
        }
        const auto now=GetTickCount64();
        int target=lastMotor;
        if(received&&got==sizeof(command)) {
            expiry=command.expiresMs;
            target=(command.motor==0||(command.motor>=10&&command.motor<=25))?int(command.motor):0;
            if(command.quit){quit=true;target=0;}
        }
        if(now>=expiry || expiry>now+1000 || quit)target=0;
        int result=ready?0:-10;
        const int attemptedMotor=target;
        if(ready && (target!=lastMotor || received)) {
            // Query readiness even for zero, but avoid repeatedly writing an unchanged running command.
            if(target!=lastMotor)result=toolkit.set(target);
            else if(!toolkit.active())result=-1;
            if(result==0)lastMotor=target;
            else {target=0;expiry=0;lastMotor=-1;}
        }
        if(attemptedMotor!=loggedMotor||result!=loggedResult){diagnostic("host target="+std::to_string(attemptedMotor)+" result="+std::to_string(result));loggedMotor=attemptedMotor;loggedResult=result;}
        if(received||result<0){reply.sequence=lastSeq;reply.completedMs=GetTickCount64();reply.result=result;reply.motor=lastMotor;
            std::string detail=!ready?toolkit.error:result==-1?"Start SteamVR with PSVR2Toolkit active":result<0?"Toolkit command failed: "+std::to_string(result):"Headset connected";
            strncpy_s(reply.detail,detail.c_str(),_TRUNCATE);DWORD n;WriteFile(pipe,&reply,sizeof(reply),&n,nullptr);
        }
        Sleep(ready?5:100);
    }
    if(ready)toolkit.set(0);
    if(parent)CloseHandle(parent);CloseHandle(pipe);ReleaseMutex(owner);CloseHandle(owner);return 0;
}
HapticsClient::~HapticsClient(){stop();}
bool HapticsClient::start(const fs::path& capi){
    if(process_)return true;
    faulted=false;faultReason.clear();sequence_=0;lastAck_=0;acknowledgedMotor=0;connected_=false;
    LARGE_INTEGER q{};QueryPerformanceCounter(&q);
    std::wstring name=L"\\\\.\\pipe\\dcs2psvr2-shaker-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(q.QuadPart);
    pipe_=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,nullptr);
    if(pipe_==INVALID_HANDLE_VALUE){status="Cannot open headset helper channel";faulted=true;return false;}
    ConnectNamedPipe(pipe_,nullptr);
    if(!spawnHidden(L"--haptics-host \""+name+L"\" \""+capi.wstring()+L"\" "+std::to_wstring(GetCurrentProcessId()),&process_)){
        CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;status="Cannot start headset helper";faulted=true;return false;
    }
    started_=GetTickCount64();status="Connecting to headset...";return true;
}
void HapticsClient::update(int motor,uint64_t now){
    if(!process_)return;
    // A caller's tick can predate start() or a reconnect. Use the current clock
    // so unsigned elapsed-time subtraction cannot create a false timeout.
    now=GetTickCount64();
    auto fault=[&](const std::string& reason){if(!faulted){faultReason=reason;diagnostic("client fault: "+reason);}faulted=true;};
    if(WaitForSingleObject(process_,0)!=WAIT_TIMEOUT){status="Headset helper exited; reconnect to continue";faulted=true;stop();return;}
    HostReply reply;DWORD n=0;
    while(ReadFile(pipe_,&reply,sizeof(reply),&n,nullptr))if(n==sizeof(reply)&&reply.magic==0x31524850){
        connected_=true;lastAck_=now;acknowledgedMotor=reply.motor;status=reply.detail;
        if(reply.result<-1)fault(reply.detail);
    }
    if(now-(lastAck_?lastAck_:started_)>(lastAck_?2000u:8000u))fault("Headset response timed out. Output state unknown; reconnect after runtime recovery.");
    HostCommand c;c.sequence=++sequence_;c.motor=faulted?0:std::clamp(motor,0,25);c.expiresMs=now+300;
    if(!WriteFile(pipe_,&c,sizeof(c),&n,nullptr)){
        const auto e=GetLastError();if(now-started_>8000&&e!=ERROR_PIPE_BUSY)fault("Headset helper communication interrupted");
    }
    if(faulted)status="Output fault: "+faultReason;
}
void HapticsClient::stop(){
    if(pipe_!=INVALID_HANDLE_VALUE){HostCommand c;c.sequence=++sequence_;c.quit=1;c.expiresMs=GetTickCount64()+300;DWORD n;WriteFile(pipe_,&c,sizeof(c),&n,nullptr);}
    // The helper observes pipe loss/parent exit and sends zero itself. Never kill a thread inside the toolkit.
    if(pipe_!=INVALID_HANDLE_VALUE){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;}
    if(process_){WaitForSingleObject(process_,750);CloseHandle(process_);process_=nullptr;}
    connected_=false;acknowledgedMotor=-1;
}
}
