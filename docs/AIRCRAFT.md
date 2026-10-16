# Aircraft profiles and flight checks

Each supported aircraft family has its own telemetry routing and saved tuning.
Mappings are candidates for live-flight testing, not a claim of headset validation.

| Profile | DCS identities | Specific behaviour |
|---|---|---|
| Hornet | `FA-18C_hornet` | Gear, speedbrake, dual afterburner and carrier mappings, plus the catapult cue. |
| Viper | `F-16C_50` | Single-engine afterburner and engine ambience; no second-engine signal required. |
| Warthog | `A-10C`, `A-10C_2` | Shared A-10C tuning; gear, speedbrake and weapon cues; no afterburner or catapult. A-10A is excluded. |
| Tomcat | `F-14B`, `F-14A-135-GR`, `F-14A-95-GR` | Shared Tomcat tuning; external speedbrake argument 400 instead of 21; dual afterburner and catapult cue. Other identity strings remain unsupported until checked. |
| Phantom | `F-4E-45MC` | Gear, speedbrake, weapons and dual afterburner. This land-based F-4E has no catapult cue. |
| Apache | `AH-64D_BLK_II` | Gun, stores, countermeasures, touchdown and measured airframe shake; optional engine/runway ambience. Fixed gear: no gear movement, configuration-airflow, afterburner or catapult output. Rotor RPM is diagnostic only. |

Each family saves its own effect tuning, Master value, profile name and gear-demo
settings. Real DCS telemetry selects the profile automatically. Without a supported
live aircraft, **Settings > Profiles and presets > Edit aircraft profile** lets you
prepare each profile. Headset calibration/ceiling, mute, Toolkit path and DCS hook
locations remain local shared settings. Import applies the imported tuning to the currently
selected aircraft; it does not replace every aircraft's settings.

Profiles start from the default headset mix with inapplicable effects disabled.
These are starting settings, not independently flight-tuned aircraft calibrations.
A-10C variants and Tomcat variants currently share family tuning.

## Signals and limits

The exporter and reference mappings are adapted from the already credited
[TelemFFB revision](https://github.com/walmis/VPforce-TelemFFB/blob/e4e4f2f4beecf8fd749ab567831a0db319a32980/export/TelemFFB.lua).
Weight on wheels uses external suspension arguments 6, 1 and 4. Jets use gear 3,
flaps 9, speedbrake 21 (Tomcat 400), and afterburner 28/29 where fitted (Viper 28
only). Carrier launch bar is 85. Finite, normalized draw values are required;
missing required values suppress dependent cues. Apache rotor RPM is read through
`BASE_SENSOR_PROPELLER_RPM` only when parameter access is available.

Flap position is exported for diagnostics; there is no separate flap-motion effect.
DCS shake is general airframe feedback, not a universal stall warning. No invented
stall threshold, Apache ETL/VRS/blade-slap cue, damage hit or ejection cue is enabled.
Unknown modules remain gated off. A future generic profile could use native common
signals with capability checks; it must not assume these six modules' draw mappings.

## Touchdown

The last airborne vertical velocity before wheel contact drives impact strength:
`clamp(descent_ft_per_min / 500, 0, 1)`. Descents below 60 ft/min produce no touchdown
cue, keeping very soft landings quiet. At 250 ft/min input strength is 50%; at or
above 500 ft/min it is 100%. Existing effect gain, response curve, Master and headset
ceiling still apply. Default command range is 10 to 25, with a 220 ms impact and
bounded recovery. There is no claim of linear physical force: the headset only
accepts bounded discrete motor commands.

Requires at least 500 ms of observed airborne telemetry, descending contact and
cooldown. Rearming, joining a mission, a stale gap and changing aircraft cannot
create a landing event. DCS vertical speed is world-relative; a moving carrier deck
can change actual impact speed. This is a descent-rate estimate, not a measured
impact impulse or carrier-relative touchdown measurement.

## Catapult launch

Hornet and Tomcat only. This is an inferred launch, not a native catapult event.
Arming requires observed wheel contact, launch bar above 0.5, speed below 25 m/s
and forward acceleration below 0.3 g in magnitude for at least 500 ms. Within a
30-second arming window, forward acceleration above 1.5 g and increasing ground
speed for 100 ms start the strong cue. It continues while ground contact and
forward acceleration above 1 g remain, capped at four seconds. Missing signals,
liftoff, a stale feed or a session/aircraft change stop it, with a short bounded fade.
A spent launch cannot repeatedly trigger during one shot.

Default peak is motor command 25, priority 95, below touchdown and above gunfire.
Master/mute and your calibrated ceiling still apply. A normal runway acceleration
or simply lowering the launch bar should not trigger it. These thresholds and
launch-bar timing need actual carrier testing, particularly Tomcat kneeling and
Hornet launch-bar retraction. Aborting setup and re-arming also need checking.

## Windows flight QA

1. Install the app, or build it using the README instructions. Back up your settings.
   Use **Settings > DCS integration > Install export (or Repair export)** to deploy the
   exporter and bridge, then restart DCS before starting a mission. After an update, repair
   the export: an installed exporter does not update itself.
2. Verify the Aircraft status identifies the module and the matching saved profile
   loads. Change a gain, switch aircraft, then return and restart the application;
   confirm each family's tuning, local ceiling and mute behave as expected.
3. Test gear, speedbrake, gun, stores/jettison and countermeasures where fitted.
   Check Viper afterburner with one engine and both engines on Hornet/Tomcat/Phantom.
   A-10C and Apache must never emit afterburner effects.
4. Fly touchdowns near 30, 100, 250, 500 and 700 ft/min. Isolate Touchdown first:
   the butter landing should be quiet, moderate descents progressively stronger,
   and 500/700 equally capped. Then test with other cues enabled and on a carrier.
5. Test both carrier aircraft: deck taxi, hookup/tension, aborted launch, normal
   catapult launch, pause/reconnect during launch, repeated sorties and normal
   runway takeoff. Only the catapult shot should produce the strong launch cue.
6. Apache: test pilot and CPG seats, gun bursts, stores, countermeasures, hover,
   takeoff/landing and ground taxi. Gear movement, afterburner and catapult must
   remain unavailable. Inspect whether shake/contact/rotor diagnostics are present.
7. Pause, leave/rejoin a mission, switch seats/aircraft and test multiplayer export
   restrictions. Missing/stale data must stop output without delayed event bursts.

Record the exact aircraft identity, DCS version, mission/carrier, seat, measured
sink rate, enabled cues, expected/actual response and relevant diagnostic values.
A screenshot of the expanded cue's activity helps distinguish a missing signal
from a suppressed or muted cue. Keep raw personal logs out of public commits.
