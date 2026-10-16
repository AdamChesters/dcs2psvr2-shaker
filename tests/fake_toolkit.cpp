// Test-only C API double. Never installed or distributed with the application.
#include "fake_toolkit.hpp"
#include <cstdint>
static HANDLE mapping=nullptr;
static FakeMotorState* state=nullptr;
extern "C" __declspec(dllexport) int psvr2_toolkit_init(){
    wchar_t name[256]{};if(!GetEnvironmentVariableW(fakeMapEnv,name,256))return -2;
    mapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name);if(!mapping)return -2;
    state=static_cast<FakeMotorState*>(MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(FakeMotorState)));return state?0:-2;
}
extern "C" __declspec(dllexport) void psvr2_toolkit_deinit(){if(state)UnmapViewOfFile(state);if(mapping)CloseHandle(mapping);state=nullptr;mapping=nullptr;}
extern "C" __declspec(dllexport) bool psvr2_toolkit_get_driver_active(){return state&&state->active!=0;}
extern "C" __declspec(dllexport) int psvr2_toolkit_set_hmd_rumble(uint8_t motor){
    if(!state)return -2;InterlockedIncrement(&state->calls);if(state->result)return state->result;
    InterlockedExchange(&state->motor,motor);return 0;
}
