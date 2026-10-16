-- Run with Lua 5.1 / LuaJIT from the repository root. No DCS or DLL is loaded.
local source=arg[1] or 'dcs/Export.lua'
local function run(name, missing, bad)
    local packet, errors, chained=nil,0,0
    local env=setmetatable({}, {__index=_G});env._G=env
    env.require=function(n) assert(n=='lfs');return {writedir=function() return '' end} end
    env.package={loadlib=function() return function() return {
        publish=function(p)packet=p end,reset=function()end,close=function()end
    } end end}
    env.log={INFO=1,write=function(_,_,msg)if msg:find('disabled')then errors=errors+1 end end}
    env.LoGetModelTime=function()return 1 end
    env.LoGetSelfData=function()return {Name=name}end
    env.LoGetAircraftDrawArgumentValue=function(i)
        if i==missing then return nil end
        if bad then return 0/0 end
        local values={[1]=0,[4]=0,[6]=.1,[3]=.4,[9]=.3,[21]=.2,[400]=.8,[28]=.7,[29]=.6,[85]=1,[25]=0}
        return values[i] or 0
    end
    env.LoGetEngineInfo=function()return {RPM={left=80,right=70}}end
    env.LoGetPayloadInfo=function()return {Cannon={shells=100},Stations={{count=2},{count=3}}}end
    env.LoGetSnares=function()return {flare=10,chaff=20}end
    env.get_param_handle=function()return {get=function()return 289 end}end
    env.LuaExportAfterNextFrame=function()chained=chained+1 end
    local chunk=assert(loadfile(source));setfenv(chunk,env);chunk();env.LuaExportStart();env.LuaExportAfterNextFrame()
    assert(packet and errors==0 and chained==1)
    local function value(key)return tonumber(packet:match('"'..key..'":([%d%.%-eE]+)'))end
    return value,packet
end
for _,name in ipairs({'FA-18C_hornet','F-16C_50','A-10C','A-10C_2','F-14B','F-14A-135-GR','F-14A-95-GR','F-4E-45MC','AH-64D_BLK_II'})do
    local v=run(name)
    assert(v('on_ground')==1 and v('cannon_rounds')==100 and v('stores_count')==5)
    if name=='AH-64D_BLK_II' then
        assert(v('gear')==nil and v('flaps')==nil and v('airbrake')==nil and v('ab_left')==nil and v('rotor_rpm')==289)
    else
        assert(v('gear')==.4 and v('flaps')==.3)
        assert(v('airbrake')==(name:find('F%-14') and .8 or .2))
        if name:find('A%-10')then assert(v('ab_left')==nil)
        elseif name=='F-16C_50'then assert(v('ab_left')==.7 and v('ab_right')==nil)
        else assert(v('ab_left')==.7 and v('ab_right')==.6)end
    end
    local missing=run(name,6);assert(missing('on_ground')==nil)
    local bad=run(name,nil,true);assert(bad('on_ground')==nil and bad('gear')==nil)
end
local v=run('F-14B',29);assert(v('ab_left')==nil and v('ab_right')==nil)
v=run('F-16C_50',29);assert(v('ab_left')==.7)
v=run('unknown');assert(v('gear')==nil and v('on_ground')==nil and v('cannon_rounds')==100)
print('PASS: module exporter mappings, helicopter exclusions, missing/invalid signals and hook chaining')
