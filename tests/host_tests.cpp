#include "haptics.hpp"
#include "core.hpp"
#include "fake_toolkit.hpp"
#include <iostream>
#include <stdexcept>
using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("Check failed: ")+ #x +" at line "+std::to_string(__LINE__));}while(0)
int wmain(int argc,wchar_t**argv){
    const auto testLog=appDirectory()/L"host-tests.log";SetEnvironmentVariableW(L"DCS2PSVR2_SHAKER_TEST_LOG",testLog.c_str());
    if(argc==5 && std::wstring(argv[1])==L"--haptics-host"){
        const auto owner=L"Local\\dcs2psvr2-shaker_TestWriter_"+std::wstring(argv[4]);
        return runHapticsHost(argv[2],argv[3],std::stoul(argv[4]),owner.c_str());
    }
    try{
        const auto dll=appDirectory()/L"fake_toolkit.dll";
        writeTextAtomic(appDirectory()/L"compatibility.json",Json({{"toolkit",Json::array({{{"sha256",sha256(dll)},{"abi","int-results-v1"}}})}}).dump());
        const auto name=L"Local\\dcs2psvr2-shaker_FakeMotor_"+std::to_wstring(GetCurrentProcessId());
        auto mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(FakeMotorState),name.c_str());CHECK(mapping);
        auto state=static_cast<FakeMotorState*>(MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(FakeMotorState)));CHECK(state);*state=FakeMotorState{};
        SetEnvironmentVariableW(fakeMapEnv,name.c_str());HapticsClient client;const auto beforeConnect=GetTickCount64();Sleep(5);CHECK(client.start(dll));
        client.update(0,beforeConnect);CHECK(!client.faulted); // The app tick was captured before helper creation.
        auto pump=[&](int motor,int duration){auto start=GetTickCount64();while(GetTickCount64()-start<uint64_t(duration)){client.update(motor,GetTickCount64());Sleep(10);}};
        pump(0,400);CHECK(client.status=="Headset connected");CHECK(!client.faulted);
        pump(12,120);CHECK(state->motor==12);CHECK(client.acknowledgedMotor==12);
        Sleep(500);CHECK(state->motor==0); // Helper stops even if the app worker stops refreshing its lease.
        pump(14,100);CHECK(state->motor==14);pump(0,100);CHECK(state->motor==0);
        InterlockedExchange(&state->active,0);pump(15,80);CHECK(state->motor==0);CHECK(client.status!="Headset connected");
        InterlockedExchange(&state->active,1);pump(0,80);CHECK(client.status=="Headset connected");
        InterlockedExchange(&state->result,-3);pump(16,80);CHECK(client.faulted);CHECK(state->motor==0);
        InterlockedExchange(&state->result,0);pump(20,100);CHECK(state->motor==0);CHECK(client.status.find("Output fault:")==0); // A fault cannot be hidden by a later successful zero.
        client.stop();Sleep(150);SetEnvironmentVariableW(fakeMapEnv,nullptr);UnmapViewOfFile(state);CloseHandle(mapping);
        std::cout<<"PASS: isolated headset host, lease expiration, explicit stop, inactive runtime and latched command failure\n";return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
