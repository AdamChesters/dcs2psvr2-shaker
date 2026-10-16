# Installation and first flight

## Requirements

- Windows x64 and DCS World with a [supported aircraft](AIRCRAFT.md).
- PSVR2 connected through its supported PC setup, with SteamVR working.
- A compatible experimental PSVR2Toolkit driver and its matching CAPI DLL.
- Headset rumble unlocked for the current power session. See the upstream [installation guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation) and [headset-unlock guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Jailbreaking-your-headset).

## Setup checklist

Open **Setup** above the status lights, or **Headset > Connection help > Open setup checklist**. The upstream projects are the source of truth for requirements and detailed instructions.

Install once:

1. Choose the newest compatible build from the [PSVR2Toolkit releases page](https://github.com/BnuuySolutions/PSVR2Toolkit/releases), including experimental releases, and follow its [installation guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation).
2. Follow the [headset jailbreak guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Jailbreaking-your-headset), including firmware requirements. Download and extract the Windows package from [vr2jb releases](https://github.com/BnuuySolutions/vr2jb/releases).
3. Install the dcs2psvr2-shaker DCS export hook in **Settings > DCS integration**. Restart DCS if it was open.

After each headset power-on, follow this order:

1. Keep SteamVR and the PlayStation VR2 App closed. Power on the headset, run `vr2jb.exe` from its extracted folder and wait for success. Follow the upstream guide if it fails.
2. Start SteamVR and wait for the headset to connect.
3. Open dcs2psvr2-shaker. Use Tests to confirm that you feel vibration; the app cannot automatically validate the jailbreak.
4. Start DCS, enter a supported aircraft, unpause, check telemetry/aircraft lights and unmute output.

For help, [open a GitHub issue](https://github.com/AdamChesters/dcs2psvr2-shaker/issues/new), or ask on [Discord](https://discord.gg/fs4WyaQPA) or [Reddit](https://www.reddit.com/r/AdamChsLab/). dcs2psvr2-shaker does not install Toolkit, flash firmware or run the jailbreak automatically.

## First use

1. Run the setup executable. It installs for the current Windows user without administrator rights. A portable ZIP is also available.
2. Open **Settings → DCS integration** and install the export into your DCS Saved Games profile. Restart DCS if it was running. Installation keeps a backup of the previous `Export.lua` and appends an identified hook; existing exporters remain in place.
3. Follow the headset startup steps above. Start a supported aircraft mission. Output follows advancing simulator time and stops on stale telemetry; there is no live-output checkbox or test-confirmation requirement.
4. In **Effects**, use Master, Mute and each row's switch/peak strength. **Test** auditions a cue alone; **Tune** expands its options. **Headset → Test headset** and **Demo flight** are available whenever you want to explore the output.
5. Adjust your enabled cues and strengths in **Effects**, or choose a preset in **Settings → Profiles and presets**. See the [effect guide](EFFECTS.md).

The gear **Test** uses both directions and excludes gear drag. **Tune → Demo travel per direction (s)** controls audition length; live travel follows DCS. **Rhythm** contains the start/lock hits, coast gaps and travel ramp. **Apply gear preset** selects 4.8-second demo travel per direction (12.2 seconds total). Live gear movement always follows telemetry.

## Status lights and tests

The status lights remain visible on every page. Version appears first.

- **Headset** checks for a connected PSVR2 USB device, without opening it. USB presence does not prove tracking, power state or unlocked rumble.
- **Jailbreak** remains **Unverified**: the Toolkit API does not expose a jailbreak-status query. Run the jailbreak before SteamVR, then use Tests to confirm physical feedback. A successful command acknowledgement is not proof of physical vibration.
- **SteamVR** detects its `vrserver.exe` process. This does not prove headset or Toolkit readiness.
- **DCS** detects the `DCS.exe` flight client, including menus and paused flight. A dedicated server does not count. Telemetry is checked separately.
- These OS checks refresh about once a second, independently of the motor loop. Missing items show a short helper; failed checks show Unknown, not a false success. No status check launches or unlocks anything.
- **Version** is green when current; a newer release makes the light flash yellow and the text turn yellow. Click it for update details. Checking or unavailable status is grey.
- **DCS hook** checks the marked export entry and both owned Lua/DLL files in detected/saved profiles. A partial installation is not marked installed. If only some profiles have the hook, the indicator reports the count.
- **DCS telemetry** lights for recent shared-memory packets with an advancing simulator clock. It returns to waiting when DCS pauses, stops or goes stale.
- **Aircraft** additionally requires a flying state, aircraft identity and numeric signals. Unsupported aircraft are labelled explicitly; only the six configured aircraft families drive effects. Individual cues require their own signals.

Tests never light the real DCS indicators. The direct headset test plays four 500 ms bursts with 250 ms gaps at the selected strength, independently of the flight ceiling. Effect tests and Demo flight respect Master and the ceiling. Pressing a test button unmutes; **STOP** mutes and cancels it. **Stop test** ends the test and lets available DCS effects resume.

**Headset → Demo flight** follows your enabled mix: ground roll → gear up → gun burst → airborne buffet → airbrake airflow → afterburner ignition and sustained rumble → store release → countermeasures → gear down → touchdown → finish. The timeline highlights the current stage and marks completed ones. Its timings come from the same schedule as the generated telemetry, using your saved gear travel time. It waits for the headset before starting. Optional engine/runway cues stay quiet unless enabled. No DCS mission is needed.

## Flight operation

Launch the app after completing the headset unlock and starting SteamVR. It opens the headset connection when DCS sends live Hornet telemetry. Windows startup is not offered because the unlock must come first. Closing the window leaves the app in the tray, with its graphics resources released. Exit completely using **Settings → Exit application** or the tray menu.

The headset must still be awake and unlocked, and SteamVR must be ready. This is a current PSVR2Toolkit hardware prerequisite; the installer cannot eliminate it. It is not necessary to start TelemFFB, SimHaptic, or a separate telemetry server.

## Stop and reconnect

Use **STOP**, the tray's stop menu, or **Ctrl+Alt+Space**. The app reports if another program already owns the shortcut. Clear Mute explicitly to resume. A telemetry cutoff defaults to 300 ms. A separate output helper has a 300 ms command lease and sends zero when the app stops refreshing it.

If the toolkit times out, output is faulted until reconnect. A driver hung inside a call can prevent a physical stop; neither this app nor a successful API response measures the motor. Recover the VR runtime before reconnecting.

## Profiles and diagnostics

Settings save under `%LOCALAPPDATA%\dcs2psvr2-shaker`. Exported tuning profiles omit machine paths and mute state. Recording and replay are not included.

Diagnostic reports include toolkit hash, application status and current effect signals. Review a report before sharing it. No telemetry or tuning is uploaded. The app contacts GitHub to check public releases at launch; an installer is downloaded only when you choose to update.

## Updating

The **Version** status light and text are green when no newer release is published. When an update is available, its light flashes yellow and its text stays yellow. It sits with the connection lights in a grid that uses four, two or one column as space allows. Alpha releases are included. Click the indicator to review the release, then choose **Download and install**. Close DCS before installing so it can release the export bridge DLL. The app verifies the downloaded installer's size and SHA-256 against GitHub release metadata, then opens the installer and exits. Your saved tuning and installation folder are retained; select **Open dcs2psvr2-shaker** at the end to restart the app. Updating does not replace the headset unlock or SteamVR startup steps.

If the check fails, the indicator stays grey, with **Check again** and **Open Releases** available when clicked. A release without a verifiable installer can be downloaded manually from its page. Portable users can install into the current folder or update manually from the ZIP. Checks use GitHub's public API without an account or token; network errors and rate limits do not interrupt flight effects.

## Removal

Use **Settings → DCS integration → Remove our export** for a selected DCS profile, or uninstall the application. Only the identified dcs2psvr2-shaker hook and its owned files are removed. The original export backup and your tuning files are retained. Close DCS before uninstalling so Windows can release the loaded bridge DLL.

## Current limitations

This is an early preview. Individual Hornet signals need in-flight validation across missions and multiplayer export policies. Runway texture and touchdown severity are approximations; carrier-relative velocity is not available. Stores-count changes cannot distinguish release from jettison. Toolkit DLL builds require an entry in the compatibility list because earlier versions exported identical names with incompatible return types. Do not simply bypass this check for an untested binary.

## Test timing and review updates

Open **Tests** for adjustable pulses, sweeps and background-layer comparisons.
All command graphs share a six-second scale. See [hardware tests](HARDWARE.md#haptic-test-pane).

After an update, the installed changelog opens once on the next launch, including
when offline. Reopen it through **Version > Installed changelog**. Fresh installs
record the current version without an update popup.
