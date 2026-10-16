# PSVR2 headset capabilities

## Interface

PSVR2Toolkit provides a single headset motor command: 10–25, with zero stopping output. Commands 1–9 map to 10; values above 25 are rejected. Upstream names the parameter `rumbleHz`, but dcs2psvr2-shaker labels it **strength** because actual frequency and force have not been measured. See the pinned [C API implementation](https://github.com/BnuuySolutions/PSVR2Toolkit/blob/629a224a7a63d2707935e19fb575d8f182305d7d/projects/psvr2_toolkit_capi/psvr2tk_capi.cpp#L87).

The headset interface does not expose independent amplitude, phase, braking or PCM audio. Controller PCM APIs do not apply to the headset. See the [driver command implementation](https://github.com/BnuuySolutions/PSVR2Toolkit/blob/629a224a7a63d2707935e19fb575d8f182305d7d/projects/psvr2_openvr_driver_ex/command_thread.cpp#L56).

## Physical interpretation

Sony's headband teardown is consistent with a single eccentric rotating mass (ERM) assembly. This is a working identification, not a verified production-part specification. A motor datasheet, measured response curve, duty rating and hardware watchdog have not been established. See [Sony's teardown](https://blog.playstation.com/2023/02/14/look-inside-playstation-vr2-with-new-teardown-videos-first-look-at-internal-components-with-engineers/) and [the teardown video](https://www.youtube.com/watch?v=NYhngu66Ccc&t=720s).

For an ERM, rotating-force amplitude follows `F = m r (2 pi f)^2`; speed and force are coupled. Perceived vibration also depends on headband fit and mechanical transmission. This relationship does not establish a measured command-to-force curve for PSVR2. [Precision Microdrives ERM explanation](https://www.precisionmicrodrives.com/ab-004).

## Output shaping

The app maps telemetry into bounded command envelopes. Impacts and ignition have finite durations and retrigger recovery. Gunfire becomes a sustained burst cue. Continuous effects use smoothing and limited stop fades. A priority mixer selects the highest-priority active cue. Background effects keep advancing underneath and resume on the next control tick when a foreground event ends. Gear retains deliberate internal quiet gaps; separate motor frequencies are not summed.

Zero-command gaps let the motor coast between sensations; they are not active braking. The response-curve control adjusts intermediate command values while preserving zero and the selected endpoints. It is a subjective tuning control, not calibrated force compensation. See [effect timings](EFFECTS.md).

## Testing your headset

**Headset → Test headset** sends four 500 ms bursts separated by 250 ms gaps. This direct test uses the selected 10–25 command independently of the flight ceiling. Cue tests and Demo flight respect Master and the ceiling.

Use **Fine-tune your useful range** to select the lowest useful command and preferred ceiling, then fit the cues to that range. Hardware acknowledgements confirm API completion, not physical motor movement. Objective frequency and rise/decay measurements require an external sensor and repeatable mounting.

## Haptic test pane

**Tests** provides steady rumble, sweep, single pulse, double knock, pulsing rumble
and pulse over rumble. Click a pattern name to play it. Every graph uses the same
six-second time scale and 0-25 command scale. Timing controls use 20 ms steps;
Windows scheduling and motor response can add delay.

Strength and timing sliders change the next playback. Controls for the playing
row are locked until it ends. All patterns include a half-second lead-in and stop
by 5.5 seconds. The layered test holds a background command, raises it for a pulse
at two seconds, then returns directly to the background without a zero command.
Graphs and playback use the same command function, including the selected ceiling.
They show requests, not measured motor motion.

These are direct command tests: the ceiling and Stop/mute apply; flight Master
and response curves do not. Test parameters last for this app session. DCS output
is paused while this pane is open. Stop test cancels playback and leaves the pane
quiet. Leaving the pane cancels playback and returns to normal flight handling.
Use the global STOP button to mute all output. Test completion never queues another
test. Physical response, minimum distinguishable pulse and gap remain unmeasured.
