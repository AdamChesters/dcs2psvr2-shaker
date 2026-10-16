# Headset effects

Effects use bounded motor commands and envelopes suited to a single headset motor. Strength settings are not measured hertz or acceleration.

## Everyday controls

**Effects** lists the selected aircraft profile's supported cues alphabetically, including runway bumps and engine ambience. Aircraft capabilities determine which rows appear; temporarily missing telemetry does not hide rows. Each row has an enable switch, peak strength and **Test**. Test plays the selected cue alone, even if its switch is off, without changing that switch. Master response and the headset ceiling still apply. **STOP** mutes all output and cancels tests. **Stop test** ends an effect audition; available DCS flight output resumes unless muted.

**Tune** expands a cue's description and optional timing, response/mixing, and command-preview panels. These panels start closed. Runway bumps and engine ambience remain off by default. The preview plots the actual engine's synthetic motor commands, not a recording of physical motor movement.

**Headset** contains connection, the direct four-burst test, Demo flight with a highlighted timeline, and the flight ceiling. **Settings** contains profiles and DCS integration, with diagnostics inside an expandable section. Effects run automatically with valid supported-aircraft telemetry; there is no live-output toggle or required test.

Each row has an activity light: green **Active** means its layer is active, even if another cue wins. A pulsing ring and **Output** label mark the current motor command. **Gap** marks an intentional quiet phase that still holds priority. Muting, an output fault or a direct motor test removes the output marker; live signal activity may remain visible. Auditions use test signals.

The selected aircraft profile is shown above the list. Apache hides afterburner, catapult, gear movement and gear/brake airflow; A-10C hides afterburner and catapult. Catapult appears only for Hornet and Tomcat. The profile follows live aircraft automatically; choose a profile in Settings for offline tuning. Hidden effects retain their saved settings.

Strength and graphs show requested motor commands, not measured vibration. No activity light proves the physical motor moved.

## Default cues

Numbers below assume master 100%, curve 1 and ceiling 25. A range is the available command span; telemetry severity and gain determine the actual value.

| Cue | Default | Shape and strength | Detection / reason for inclusion |
|---|---|---|---|
| Catapult launch | On for Hornet/Tomcat | 20-25, strong bounded launch rumble | Inferred from launch-bar setup and sustained acceleration; not a direct catapult event. |
| Gear up/down | On | 200 ms hit at 20 → 300 ms quiet → rising travel at 14–16 → 460 ms quiet → 250 ms hit at 19 | Position changes determine travel. Animation endpoints approximate completion. |
| Gun burst | On | 20–25 while ammunition falls; 180 ms hold after the last observed decrease, 60 ms settle, 300 ms retrigger recovery | A sustained firing cue, since individual rounds arrive too quickly for separate motor pulses. |
| Touchdown | On | 10-25, 220 ms hit, 140 ms settle, 350 ms retrigger recovery | Descending contact after at least 500 ms airborne, with a 1.1 s cooldown. Strength scales linearly to full at 500 ft/min; below 60 ft/min stays quiet. Carrier-relative velocity is unavailable. |
| Afterburner onset | On | Kick at 21 for 180 ms, then roughly 14–16 rumble fading over 700 ms; 350 ms quiet | Engagement of either engine gives one combined cue. Hysteresis plus 1.4 s cooldown prevents repeated throttle chatter. Joining a flight already in AB gives no invented ignition. |
| Afterburner rumble | On | Continuous 12–16; 250 ms attack, 120 ms release, stop fade capped at 300 ms | Follows the stronger afterburner signal while either engine remains engaged. Independent of the onset cue and normal engine ambience. |
| Store release | On | Sharp 160 ms pulse at 20, then 280 ms quiet | Falling airborne store count. Includes jettison; does not identify a weapon or prove a successful launch. |
| Countermeasures | On | Sharp 140 ms pulse at 15, then 260 ms quiet | Falling flare or chaff count. Rapid releases coalesce into bounded cues rather than queue. |
| Airborne buffet | On | 12–18, 80 ms attack / 120 ms release; stop fade capped at 260 ms | General DCS shake while airborne. Useful airframe feedback, not an isolated stall or G-force warning. |
| Gear/brake airflow | On | Subtle 11–13, 180 ms attack / 120 ms release; stop fade capped at 320 ms | Airspeed and deployment while airborne. Kept separate from gear movement; yields during the mechanism's quiet phases. |
| Engine ambience | Off, available | Low 10–12, 400 ms attack / 160 ms release; stop fade capped at 450 ms | Optional RPM feedback for users without seat/stick haptics. Does not reproduce engine harmonics. |
| Runway bumps | Off, available | Low 10–12, 50 ms attack / 80 ms release; stop fade capped at 200 ms | Changing vertical acceleration while rolling. Steady acceleration produces no invented continuous buzz. |
| Damage / ejection | Unavailable | No output | Damage animation is not a reliable individual hit event. Ejection requires a validated own-aircraft event source. |

Store and countermeasure events arriving during their hit/recovery are consumed, never queued for a delayed pulse. A later event after recovery can produce another cue. Gunfire intentionally extends a burst while new rounds are observed. Missing required signals immediately stop their cue and establish fresh baselines when restored.

## Inertia and mixing

Impact and ignition cues have finite envelopes. When they end, the highest-priority active background resumes on the next control tick, normally about 20 ms, without a forced zero command. Retrigger recovery limits new impacts but does not silence an active background. Gear's internal quiet gaps remain part of its pattern. Priorities are touchdown 100, catapult 95, gun 90, stores 80, AB onset 60, countermeasures 50, gear 45, airflow 35, sustained AB 30, buffet 25, runway 15 and engine 5.

Continuous effects receive smoothing and bounded stop fades, rather than repeated gear-like impacts. The engine selects one dominant cue; it does not add motor frequencies or sum every effect into saturation. These defaults accommodate perceived inertia without claiming measured spin-up, braking or inverse physical compensation.

For gear/brake airflow, deployment must be nonzero, the aircraft must be airborne, and indicated airspeed must exceed 40 m/s (about 78 knots). Intensity rises with deployment and speed. It remains active after deployment finishes; full deployment reaches its maximum input at 140 m/s (about 272 knots). Partial deployment gives less input, and the response threshold may suppress very low input.

If raising a cue's strength makes no difference, open **Tune → Command preview and activity** during flight. **Background suppressed** means another cue has priority; **Below threshold** means its signal is too low. Airborne buffet yields to gear, countermeasures, afterburner and configuration airflow. It only responds when the module reports airborne shake; it is not an always-on rumble or a dedicated stall/high-alpha detector. A stronger signal does not override another effect's higher priority.

Afterburner rumble follows the current engine signal, so joining or reconnecting during afterburner can resume the rumble without inventing an ignition kick. Sustained rumble resumes as soon as the onset envelope ends. Leaving afterburner fades the rumble; missing signals or stale telemetry stop it.

## Profiles and gear preset

Settings save automatically. **Settings → Profiles and presets → Apply default headset mix** applies the flight-cue defaults while keeping your gear tuning, demo travel, Master, ceiling and response curve. Profile import fits effect ranges to your local headset range.

**Apply gear preset** updates the gear cue with a start hit, quiet gap, rising travel rumble, coast and endpoint hit. It selects 4.8 seconds of demo travel per direction; you can set this from three to fifteen seconds. This controls the audition only. Live gear timing follows DCS telemetry.

## Multi-aircraft candidate changes

See [aircraft profiles](AIRCRAFT.md) for capability differences and per-family saved tuning. Touchdown scales linearly to full at 500 ft/min, with no cue below 60 ft/min and a default command range of 10-25. Hornet/Tomcat catapult launch uses a strong bounded cue at priority 95. New mappings require live-flight QA.
