-- dcs2psvr2-shaker independent telemetry exporter. GPL-3.0.
-- DCS signals/aircraft draw arguments reference TelemFFB (Valmantas Paliksa,
-- Micah Frisby and contributors); see THIRD_PARTY_NOTICES.md.
-- Install through the application. Existing exporters are chained, never replaced.
if _G.Dcs2Psvr2ShakerExport then return end
local state = { last = -1, failed = false }
-- Hornet damage draw arguments adapted from TelemFFB's FA-18 mapping (GPL-3.0).
-- Duplicates in the upstream sum are omitted. This is an indicator, not a hit event.
local damageArgs = {65,135,136,137,146,148,149,150,152,153,154,156,157,158,160,166,183,213,214,215,216,217,220,222,223,224,225,226,227,230,232,233,235,241,242,244,245,247,248,265,266,267,298,299}
-- Exact module IDs: never apply one aircraft's draw arguments to an unknown module.
-- Shared external arguments and exceptions follow the credited TelemFFB revision.
local hornet = { gear=3, flaps=9, brake=21, ab={28,29}, carrier=true, damage=true }
local viper = { gear=3, flaps=9, brake=21, ab={28} }
local warthog = { gear=3, flaps=9, brake=21 }
local tomcat = { gear=3, flaps=9, brake=400, ab={28,29}, carrier=true }
local phantom = { gear=3, flaps=9, brake=21, ab={28,29} }
local apache = { helicopter=true }
local profiles = {
    ['FA-18C_hornet']=hornet, ['F-16C_50']=viper,
    ['A-10C']=warthog, ['A-10C_2']=warthog,
    ['F-14B']=tomcat, ['F-14A-135-GR']=tomcat, ['F-14A-95-GR']=tomcat,
    ['F-4E-45MC']=phantom, ['AH-64D_BLK_II']=apache
}
_G.Dcs2Psvr2ShakerExport = state
local lfs = require('lfs')
local root = lfs.writedir() .. 'Scripts/dcs2psvr2-shaker/'
local loadBridge, bridgeError = package.loadlib(root .. 'dcs2psvr2-shaker-bridge.dll', 'luaopen_dcs2psvr2_shaker_bridge')
local bridge
if loadBridge then local ok, value = pcall(loadBridge); if ok then bridge = value end end
local function report(message)
    if log and log.write then log.write('dcs2psvr2-shaker', log.INFO, tostring(message)) end
end
if type(bridge) ~= 'table' or type(bridge.publish) ~= 'function' then
    report('Bridge unavailable: ' .. tostring(bridgeError)); return
end
local function read(fn, ...)
    if type(fn) ~= 'function' then return nil end
    local ok, value = pcall(fn, ...)
    if ok then return value end
end
local function finite(n) return type(n) == 'number' and n == n and n ~= math.huge and n ~= -math.huge end
local function quote(s)
    return '"' .. tostring(s):gsub('[%z\1-\31\\"]', function(c)
        if c == '"' then return '\\"' elseif c == '\\' then return '\\\\' end
        return string.format('\\u%04x', string.byte(c))
    end) .. '"'
end
local function encode(values)
    local keys = {}; for key in pairs(values) do keys[#keys+1] = key end; table.sort(keys)
    local items = {}
    for _,key in ipairs(keys) do
        local value = values[key]
        if finite(value) then items[#items+1] = quote(key) .. ':' .. string.format('%.8g', value):gsub(',', '.') end
    end
    return '{' .. table.concat(items, ',') .. '}'
end
local function publish()
    local t = read(LoGetModelTime)
    if not finite(t) then return end
    if t >= state.last and t - state.last < 1/50 then return end
    if t < state.last then bridge.reset() end
    state.last = t
    local aircraft = read(LoGetSelfData)
    if type(aircraft) ~= 'table' or type(aircraft.Name) ~= 'string' then
        bridge.publish('{"version":1,"state":"no_aircraft","aircraft":"","values":{}}', t); return
    end
    local v = {}
    local function put(k,n) if finite(n) then v[k] = n end end
    put('shake', read(LoGetShakeAmplitude))
    put('aoa_deg', read(LoGetAngleOfAttack))
    put('ias_mps', read(LoGetIndicatedAirSpeed))
    put('vertical_mps', read(LoGetVerticalVelocity))
    local acc = read(LoGetAccelerationUnits)
    if type(acc) == 'table' then put('accel_x_g',acc.x); put('accel_y_g',acc.y); put('accel_z_g',acc.z) end
    local vel = read(LoGetVectorVelocity)
    if type(vel) == 'table' and finite(vel.x) and finite(vel.z) then put('ground_mps',math.sqrt(vel.x*vel.x + vel.z*vel.z)) end
    local engine = read(LoGetEngineInfo)
    if type(engine) == 'table' and type(engine.RPM) == 'table' then put('rpm_left_pct',engine.RPM.left); put('rpm_right_pct',engine.RPM.right) end
    local payload = read(LoGetPayloadInfo)
    if type(payload) == 'table' then
        if type(payload.Cannon) == 'table' then put('cannon_rounds',payload.Cannon.shells) end
        if type(payload.Stations) == 'table' then
            local count, complete = 0, true
            for _, station in pairs(payload.Stations) do
                if type(station) ~= 'table' or not finite(station.count) or station.count < 0 then complete = false; break end
                count = count + station.count
            end
            if complete then put('stores_count',count) end
        end
    end
    local cm = read(LoGetSnares)
    if type(cm) == 'table' then put('flares',cm.flare); put('chaff',cm.chaff) end
    local profile = profiles[aircraft.Name]
    if profile then
        local function arg(i)
            if not i then return nil end
            local n=read(LoGetAircraftDrawArgumentValue,i)
            if finite(n) and n>=0 and n<=1 then return n end
        end
        local l,n,r = arg(6),arg(1),arg(4)
        if finite(l) and finite(n) and finite(r) then put('on_ground', (l+n+r > 0.001) and 1 or 0) end
        put('gear',arg(profile.gear)); put('flaps',arg(profile.flaps)); put('airbrake',arg(profile.brake))
        if profile.ab then
            local left=arg(profile.ab[1])
            local right=profile.ab[2] and arg(profile.ab[2]) or nil
            if left and (#profile.ab==1 or right) then
                put('ab_left',left); put('ab_right',right)
            end
        end
        if profile.carrier then put('launch_bar',arg(85)); put('tail_hook',arg(25)) end
        if profile.helicopter then
            -- Protected: parameter access is not available in every export context.
            local rpm=read(function() return get_param_handle('BASE_SENSOR_PROPELLER_RPM'):get() end)
            put('rotor_rpm',rpm)
        end
        if profile.damage then
            local damage, complete = 0, true
            for _,i in ipairs(damageArgs) do
                local value = arg(i)
                if not finite(value) then complete = false; break end
                damage = damage + value
            end
            if complete then put('damage_total',damage) end
        end
    end
    local packet = '{"version":1,"state":"flying","aircraft":' .. quote(aircraft.Name) .. ',"values":' .. encode(v) .. '}'
    if #packet <= 32768 then bridge.publish(packet,t) end
end
local function protect(fn)
    if state.failed then return end
    local ok, err = pcall(fn)
    if not ok then state.failed = true; pcall(bridge.close); report('Export disabled after error: ' .. tostring(err)) end
end
local previousStart, previousFrame, previousStop = LuaExportStart, LuaExportAfterNextFrame, LuaExportStop
LuaExportStart = function()
    if previousStart then local ok,err = pcall(previousStart); if not ok then report('Earlier start hook: ' .. tostring(err)) end end
    state.last = -1; state.failed = false; protect(function() bridge.reset() end)
end
LuaExportAfterNextFrame = function()
    if previousFrame then local ok,err = pcall(previousFrame); if not ok and not state.previousFailed then report('Earlier frame hook: ' .. tostring(err)); state.previousFailed = true end end
    protect(publish)
end
LuaExportStop = function()
    protect(function() bridge.publish('{"version":1,"state":"stopped","aircraft":"","values":{}}',state.last); bridge.close() end)
    if previousStop then local ok,err = pcall(previousStop); if not ok then report('Earlier stop hook: ' .. tostring(err)) end end
end
report('Independent telemetry exporter loaded')
