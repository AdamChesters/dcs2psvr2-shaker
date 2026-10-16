// Tiny publisher for the Lua runtime already loaded by DCS. No separate Lua VM.
#include "telemetry.hpp"
#include <mutex>

struct lua_State;
using LuaFunction = int(__cdecl*)(lua_State*);
namespace {
using GetTop = int(__cdecl*)(lua_State*);
using ToString = const char*(__cdecl*)(lua_State*, int, size_t*);
using ToNumber = double(__cdecl*)(lua_State*, int);
using Type = int(__cdecl*)(lua_State*, int);
using PushBool = void(__cdecl*)(lua_State*, int);
using PushString = void(__cdecl*)(lua_State*, const char*);
using NewTable = void(__cdecl*)(lua_State*, int, int);
using PushClosure = void(__cdecl*)(lua_State*, LuaFunction, int);
using SetField = void(__cdecl*)(lua_State*, int, const char*);
GetTop gettop; ToString tostring; ToNumber tonumber; Type type;
PushBool pushbool; PushString pushstring; NewTable newtable; PushClosure pushclosure; SetField setfield;
shaker::TelemetryWriter writer;
std::mutex gate;
template<class T> bool bind(HMODULE m, T& fn, const char* name) {
    fn = reinterpret_cast<T>(GetProcAddress(m, name)); return fn != nullptr;
}
bool bindLua() {
    HMODULE m = GetModuleHandleW(L"lua.dll");
    if (!m) m = GetModuleHandleW(L"lua51.dll");
    return m && bind(m,gettop,"lua_gettop") && bind(m,tostring,"lua_tolstring") && bind(m,tonumber,"lua_tonumber") &&
        bind(m,type,"lua_type") && bind(m,pushbool,"lua_pushboolean") && bind(m,pushstring,"lua_pushstring") &&
        bind(m,newtable,"lua_createtable") && bind(m,pushclosure,"lua_pushcclosure") && bind(m,setfield,"lua_setfield");
}
int publish(lua_State* L) {
    bool ok = false;
    if (gettop(L) == 2 && type(L,1) == 4 && type(L,2) == 3) {
        size_t n = 0; const char* s = tostring(L,1,&n);
        if (s && n && n <= shaker::payloadCapacity) {
            try {
                std::unique_lock guard(gate, std::try_to_lock);
                if (guard.owns_lock() && writer.open()) ok = writer.publish({s,n},tonumber(L,2));
            } catch (...) { ok = false; }
        }
    }
    pushbool(L,ok); return 1;
}
int reset(lua_State* L) {
    { std::unique_lock guard(gate,std::try_to_lock); if (guard.owns_lock()) writer.newSession(); }
    pushbool(L,1); return 1;
}
int close(lua_State* L) {
    { std::unique_lock guard(gate,std::try_to_lock); if (guard.owns_lock()) writer.close(); }
    pushbool(L,1); return 1;
}
}
extern "C" __declspec(dllexport) int luaopen_dcs2psvr2_shaker_bridge(lua_State* L) {
    if (!bindLua()) return 0;
    newtable(L,0,4);
    pushclosure(L,publish,0); setfield(L,-2,"publish");
    pushclosure(L,reset,0); setfield(L,-2,"reset");
    pushclosure(L,close,0); setfield(L,-2,"close");
    pushstring(L,"1"); setfield(L,-2,"protocol"); return 1;
}
