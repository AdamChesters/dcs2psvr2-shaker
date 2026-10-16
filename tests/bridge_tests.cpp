#include "telemetry.hpp"
#include "platform.hpp"
#include "core.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>

using namespace shaker;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("Check failed: ")+ #x +" at line "+std::to_string(__LINE__));}while(0)
struct lua_State;
template<class T>T symbol(HMODULE module,const char* name){auto p=reinterpret_cast<T>(GetProcAddress(module,name));CHECK(p);return p;}
static void luaTest(HMODULE bridge,const fs::path& runtime,const fs::path& exporter){
    auto lua=LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);CHECK(lua);
    auto create=symbol<lua_State*(__cdecl*)()>(lua,"luaL_newstate");
    auto close=symbol<void(__cdecl*)(lua_State*)>(lua,"lua_close");
    auto openlibs=symbol<void(__cdecl*)(lua_State*)>(lua,"luaL_openlibs");
    auto setfield=symbol<void(__cdecl*)(lua_State*,int,const char*)>(lua,"lua_setfield");
    auto load=symbol<int(__cdecl*)(lua_State*,const char*)>(lua,"luaL_loadstring");
    auto call=symbol<int(__cdecl*)(lua_State*,int,int,int)>(lua,"lua_pcall");
    auto str=symbol<const char*(__cdecl*)(lua_State*,int,size_t*)>(lua,"lua_tolstring");
    auto settop=symbol<void(__cdecl*)(lua_State*,int)>(lua,"lua_settop");
    auto state=create();CHECK(state);openlibs(state);
    auto module=symbol<int(__cdecl*)(lua_State*)>(bridge,"luaopen_dcs2psvr2_shaker_bridge");CHECK(module(state)==1);setfield(state,-10002,"bridge");
    auto eval=[&](const std::string& source){if(load(state,source.c_str())||call(state,0,0,0)){auto error=str(state,-1,nullptr);throw std::runtime_error(error?error:"Lua call failed");}};
    eval("assert(bridge.protocol == '1'); assert(not bridge.publish(5, 2)); assert(not bridge.publish('x', 'bad')); assert(not bridge.publish(string.rep('x', 32769), 1)); assert(bridge.publish('test-payload', 42.5))");
    TelemetryReader reader;TelemetryPacket packet;CHECK(reader.read(packet));CHECK(packet.payload=="test-payload");CHECK(packet.simTime==42.5);
    auto previous=packet.session;eval("bridge.reset();assert(bridge.publish('new-session', 0))");CHECK(reader.read(packet));CHECK(packet.session!=previous);
    eval("bridge.close()");CHECK(!reader.read(packet));reader.close();
    if(!exporter.empty()){
        eval(R"(
            modelTime=1; previousCalls=0
            package.preload.lfs=function() return {writedir=function() return '/test/' end} end
            package.loadlib=function() return function() return bridge end end
            LuaExportAfterNextFrame=function() previousCalls=previousCalls+1 end
            LoGetModelTime=function() return modelTime end
            LoGetSelfData=function() return {Name='FA-18C_hornet'} end
            LoGetShakeAmplitude=function() return 0.35 end
            LoGetAccelerationUnits=function() return {x=0,y=1,z=0} end
            LoGetVectorVelocity=function() return {x=30,y=0,z=40} end
            LoGetEngineInfo=function() return {RPM={left=75,right=78}} end
            LoGetPayloadInfo=function() return {Cannon={shells=450},Stations={{count=2},{count=3}}} end
            LoGetSnares=function() return {flare=30,chaff=60} end
            LoGetAircraftDrawArgumentValue=function(i) if i==6 then return 0.3 else return 0 end end
        )");
        eval(readText(exporter));eval("LuaExportStart();LuaExportAfterNextFrame()");CHECK(reader.read(packet));
        Frame f;std::string error;CHECK(parseFrame(packet.payload,f,error));CHECK(f.value("cannon_rounds")==450);CHECK(f.value("ground_mps")==50);CHECK(f.value("stores_count")==5);CHECK(f.value("on_ground")==1);CHECK(f.value("damage_total")==0);
        auto seq=packet.sequence;eval("LuaExportAfterNextFrame();assert(previousCalls==2)");CHECK(reader.read(packet));CHECK(packet.sequence==seq);
        eval("modelTime=1.1;LoGetEngineInfo=function() error('missing engine') end;LuaExportAfterNextFrame()");CHECK(reader.read(packet));CHECK(parseFrame(packet.payload,f,error));CHECK(!f.value("rpm_left_pct"));CHECK(f.value("shake")==.35);
        eval("modelTime=1.15;LoGetAircraftDrawArgumentValue=function(i) if i==152 then return 0.5 elseif i==242 then return 0.25 else return 0 end end;LuaExportAfterNextFrame()");CHECK(reader.read(packet));CHECK(parseFrame(packet.payload,f,error));CHECK(f.value("damage_total")==.75); // Duplicate upstream arguments are counted once.
        eval("modelTime=1.18;LoGetAircraftDrawArgumentValue=function(i) if i==152 then return nil else return 0 end end;LuaExportAfterNextFrame()");CHECK(reader.read(packet));CHECK(parseFrame(packet.payload,f,error));CHECK(!f.value("damage_total"));
        eval("modelTime=1.2;LoGetSelfData=function() return nil end;LuaExportAfterNextFrame()");CHECK(reader.read(packet));CHECK(parseFrame(packet.payload,f,error));CHECK(f.state=="no_aircraft");
        eval("LuaExportStop()");CHECK(!reader.read(packet));reader.close();
        std::cout<<"PASS: real Lua exporter, existing-hook chaining, frame throttle, missing APIs and session stop\n";
    }
    settop(state,0);close(state);FreeLibrary(lua);
    std::cout<<"PASS: native bridge exercised with supplied Lua runtime\n";
}
int wmain(int argc,wchar_t**argv){
    try{
        const auto name=L"Local\\dcs2psvr2-shaker_Test_"+std::to_wstring(GetCurrentProcessId());
        TelemetryWriter writer,duplicate;TelemetryReader reader(name);TelemetryPacket packet;
        CHECK(!reader.read(packet));CHECK(writer.open(name));CHECK(!duplicate.open(name));CHECK(!reader.read(packet));
        CHECK(!writer.publish("",0));CHECK(writer.publish("abc",12.25));CHECK(reader.read(packet));CHECK(packet.payload=="abc");CHECK(packet.simTime==12.25);
        auto sequence=packet.sequence,session=packet.session;writer.newSession();CHECK(writer.publish("def",0));CHECK(reader.read(packet));CHECK(packet.sequence>sequence);CHECK(packet.session!=session);
        auto mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(name+L"_Mutex").c_str());CHECK(mutex);
        std::thread holder([&]{CHECK(WaitForSingleObject(mutex,1000)==WAIT_OBJECT_0);Sleep(150);ReleaseMutex(mutex);});Sleep(30);
        const auto start=GetTickCount64();CHECK(!writer.publish("skip",1));CHECK(!reader.read(packet));CHECK(GetTickCount64()-start<50);holder.join();CloseHandle(mutex);
        writer.close();CHECK(!reader.read(packet));reader.close();CHECK(writer.open(name));CHECK(writer.publish("reopened",2));CHECK(reader.read(packet));writer.close();reader.close();
        CHECK(argc>=2);auto module=LoadLibraryW(argv[1]);CHECK(module);CHECK(GetProcAddress(module,"luaopen_dcs2psvr2_shaker_bridge"));
        if(argc>=3)luaTest(module,argv[2],argc>=4?fs::path(argv[3]):fs::path{});FreeLibrary(module);
        std::cout<<"PASS: shared memory identity, bounds, session restart, single writer, non-blocking contention and module exports\n";return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
