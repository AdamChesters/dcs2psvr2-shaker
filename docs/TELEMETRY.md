# DCS signal reference

The app owns its DCS exporter and shared-memory publisher. Other haptic applications can run independently; they are not telemetry dependencies.

| Cue | Detection | Limits |
|---|---|---|
| Gear start / travel / endpoint | Changes in the selected jet profile's external gear argument 3; direction and endpoint state machine | The animation endpoint is not a separately verified mechanical lock signal. Starting a mission does not produce a start pulse. |
| Gear / speedbrake airflow | Airborne, deployment arguments and indicated airspeed | An approximation, separate from gear movement. Aircraft-specific speed thresholds need flight tuning. |
| Gun | Falling cannon ammunition | Rearm and mission changes establish a baseline. Unlimited-ammunition missions need validation. |
| Touchdown | Descending wheel contact after at least 500 ms airborne; 1.1 s cooldown | Carrier-relative velocity is unavailable. |
| Afterburner onset | Either engine crosses the engagement threshold; hysteresis and cooldown | One combined kick/rumble. A starting or restored AB signal establishes a baseline. |
| Afterburner rumble | Maximum of the fitted engine AB draw arguments while engaged | Ongoing rumble with separate tuning. Starts from current state, including on reconnect; missing a required fitted-engine signal stops it. |
| Store release | Falling airborne stores count, only when every returned station count is valid | Includes jettison; enabled by default. Missing or malformed station values omit the whole count. |
| Countermeasures | Falling flare or chaff count | Rapid programs coalesce into bounded pulses. Rearm and missing counts do not trigger. |
| Runway bumps | Changes from a slow vertical-acceleration baseline while rolling | Optional and off. A steady signal produces no continuous rumble. |
| Damage | Export still includes a sum of Hornet damage draw arguments for diagnostics | No headset output. Animation changes are not verified hit events. |

The Hornet damage argument list and existing signal mappings are adapted from [TelemFFB's exporter](https://github.com/walmis/VPforce-TelemFFB/blob/e4e4f2f4beecf8fd749ab567831a0db319a32980/export/TelemFFB.lua#L1256), under this project's GPL-3.0 license. Duplicate upstream argument indices are counted once. Missing required damage values omit the entire sum rather than producing a false decrease/increase.


## Aircraft and session state

Only the six configured [aircraft families](AIRCRAFT.md) drive effects. The app requires recent packets, advancing simulator time, a flying state and aircraft signals. Missing values suppress dependent effects. Session changes, time resets, aircraft changes and stale gaps reset event baselines.

Airborne buffet uses DCS airframe shake while airborne. Engine ambience follows engine RPM and is disabled by default. Both are continuous cues with bounded stop fades.

Damage and ejection are unavailable because a reliable own-aircraft event source has not been validated. Loss of telemetry is never interpreted as an ejection or crash. Multiplayer export policy may suppress required signals; carrier-relative touchdown velocity is unavailable.

See [aircraft profiles](AIRCRAFT.md) for module identities, exact draw mappings, catapult inference and the 500 ft/min touchdown scaling.
