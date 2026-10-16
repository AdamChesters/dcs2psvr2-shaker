# Independent DCS telemetry bridge, version 1

The Saved Games Lua exporter loads `dcs2psvr2-shaker-bridge.dll` through DCS's existing Lua 5.1-compatible runtime. The DLL resolves only the Lua functions it uses from the loaded `lua.dll`/`lua51.dll`; it never creates a second Lua VM in DCS.

`bridge.publish(json, simTime)` returns a boolean. It publishes at most 32,768 bytes and does not wait for the GUI. `reset()` changes the session identity. `close()` retires the writer.

## Transport

- Mapping: `Local\dcs2psvr2-shaker_DCS_v1`
- Mutex: `Local\dcs2psvr2-shaker_DCS_v1_Mutex`
- Windows session-local, single writer, multiple read-only readers.
- Both publisher and reader acquire the mutex with a zero timeout. Contention skips a frame.
- The fixed, 8-byte-aligned header is defined in `src/telemetry.hpp`; payload starts at byte 56.
- Header: magic, version, region/payload lengths, writer PID, flags, session ID, sequence, monotonic publication milliseconds, simulator seconds.
- The reader validates version, bounds, clock and publisher liveness before parsing a private copy.

JSON contains `version`, `state` (`flying`, `no_aircraft`, `stopped`), `aircraft`, and numeric `values`. Missing or invalid API values are omitted. Units are included in field names where applicable. This is not a supported third-party telemetry ABI until a stable release; readers must check the version.

## Signal handling

The exporter samples at up to 50 simulator frames per second. The app independently checks advancing simulator time; repeated frames cannot refresh a paused-flight effect. New sessions, time resets, aircraft changes and gaps beyond the configured cutoff reset event baselines. A newly observed low ammunition count is not treated as gunfire.

The engine computes normalized effect envelopes and selects the highest-priority active cue for the single headset motor. Motor settings are bounded to the toolkit command range. Zero is a separate output state, not a low motor speed.

## Headset subprocess

The same executable starts with an internal `--haptics-host` argument. A local named pipe carries a sequence, motor setting and expiration timestamp. The helper takes only the newest valid request, monitors parent exit and pipe loss, and issues zero after lease expiry. No simulator callback or GUI thread calls the toolkit directly. Faults are surfaced rather than silently treated as a healthy physical device.

The helper serializes headset writes from this application. It cannot prevent an unrelated application from sending conflicting headset commands.
