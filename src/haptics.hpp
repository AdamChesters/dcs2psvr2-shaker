#pragma once
#include "platform.hpp"
#include <cstdint>

namespace shaker {
struct HostCommand {
    uint32_t magic=0x31534850, sequence=0;
    uint64_t expiresMs=0;
    uint32_t motor=0, quit=0;
};
struct HostReply {
    uint32_t magic=0x31524850, sequence=0;
    uint64_t completedMs=0;
    int32_t result=0, motor=0;
    char detail[160]{};
};
int runHapticsHost(const std::wstring& pipe,const fs::path& capi,DWORD parentPid,const wchar_t* ownerName=L"Local\\dcs2psvr2-shaker_HeadsetWriter_v1");
class HapticsClient {
    HANDLE pipe_=INVALID_HANDLE_VALUE,process_=nullptr;
    uint32_t sequence_=0;
    uint64_t lastAck_=0,started_=0;
    bool connected_=false;
public:
    std::string status="Headset output idle";
    std::string faultReason;
    int acknowledgedMotor=0;
    bool faulted=false;
    ~HapticsClient();
    bool start(const fs::path& capi);
    void update(int motor,uint64_t now);
    void stop();
    bool running()const{return process_!=nullptr;}
};
}
