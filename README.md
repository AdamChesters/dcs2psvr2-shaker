<p align="center">
  <img src="assets/social-preview.jpg" alt="dcs2psvr2-shaker. The PSVR2 has a haptic motor. Use it in DCS." width="1280">
</p>

# dcs2psvr2-shaker

Leverage DCS World haptics for the Playstation VR2 (PSVR2) headset on PC, by Adam Chesters.

Want your vision/head to literally shake when you fire the glorious F/A-18C M61A1 Vulcan 20mm six-barreled rotary cannon? 

I gotchu. 

<img src="assets/dcs2psvr2-shaker.png" alt="dcs2psvr2-shaker icon: S with vibration ripples" width="96" height="96">

> [!WARNING]
> **ALPHA SOFTWARE — EXPECT TO TEST AND TUNE.** Effects and defaults are still being refined through live flights. Some cues may feel too strong, too weak or need timing adjustments for your setup. Use the individual effect tests and tuning controls to find your preferred mix; signal detection and aircraft coverage are still limited.

Windows x64; F/A-18C Hornet, F-16C Viper, A-10C/II, F-14A/B Tomcat, F-4E Phantom and AH-64D Apache. Aircraft profiles are awaiting live-flight validation. dcs2psvr2-shaker reads DCS through its own export hook and shared-memory bridge. It works alongside seat and joystick haptics without requiring SimHaptic or TelemFFB.

## Prerequisites

- **[PSVR2Toolkit](https://github.com/BnuuySolutions/PSVR2Toolkit)** — install a compatible experimental build and **follow its [headset jailbreaking instructions](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Jailbreaking-your-headset)**, including the required firmware/setup steps. Toolkit and `vr2jb.exe` are not bundled with this app.
- A PSVR2 headset connected to a Windows x64 PC, with its PC setup and SteamVR working.
- DCS World and a [supported aircraft](docs/AIRCRAFT.md) for live flight effects.
- You **MUST** jailbreak the headset every time you power it on again, before launching SteamVR.

## Current features

- Gear movement, gun bursts, touchdown, afterburner ignition and sustained rumble, store release, countermeasures, airborne buffet and subtle gear/speedbrake airflow.
- Optional engine and runway ambience, disabled by default.
- Per-effect switches, strength controls, isolated tests and expandable timing/response controls.
- Master, Mute, headset strength ceiling and an emergency-stop shortcut.
- A direct four-burst headset test and a demo flight with a highlighted timeline, using your enabled mix and saved gear travel time.
- Automatic effect output when valid supported-aircraft telemetry and the headset are ready.
- Persistent indicators for the DCS hook, advancing simulator telemetry and aircraft data.
- Importable/exportable tuning profiles, connection diagnostics and a step-by-step `vr2jb.exe` guide.
- Per-user installer and portable ZIP. Closing the window keeps the app in the tray.
- Launch-time GitHub release checks, a green current-version / yellow update-available indicator, and a confirmed installer update that retains your tuning.

## Potential features

- More effects and richer event detection.
- Additional aircraft beyond the current six families.
- More simulators, covering both racing and flight, including potential MSFS 2024 support.

These are possibilities for future development, not supported features or promised release dates.

## Getting started

Download the installer or portable ZIP from [Releases](https://github.com/AdamChesters/dcs2psvr2-shaker/releases).

**Headset haptics WILL NOT WORK if `vr2jb.exe` has not successfully unlocked the headset for the current power session. Installing PSVR2Toolkit alone is not enough.**

**Launch order: `vr2jb.exe` → SteamVR → dcs2psvr2-shaker → DCS.**

1. Exit **DCS, SteamVR, the PlayStation VR2 App and dcs2psvr2-shaker**. To exit dcs2psvr2-shaker fully, use its tray menu or **Settings → Exit application**; the window's X only hides it.
2. Turn the headset on and keep it awake, with SteamVR still closed.
3. Run **`vr2jb.exe`** from the compatible [vr2jb release](https://github.com/BnuuySolutions/vr2jb/releases) and wait for a successful unlock. Complete the Toolkit jailbreaking instructions first; see the [setup checklist](docs/SETUP.md) for links and startup order.
4. Start **SteamVR** and wait for its headset indicator to show connected.
5. Launch **dcs2psvr2-shaker**. On first use, install the DCS hook from **Settings → DCS integration**.
6. Launch **DCS** and enter a supported aircraft mission. Effects run automatically. Master and Mute control output; tests are optional.

Repeat the unlock and launch order after powering the headset off. `vr2jb.exe` does not need to remain open.

The title area checks GitHub Releases at launch, including Alpha releases. The **Version** light and text are green when current; the light flashes yellow and the text stays yellow when an update is available. The status grid also shows headset USB presence, jailbreak guidance, SteamVR, DCS and telemetry. Open **Setup** above it for a concise checklist and upstream links. Click it to review the version and choose **Download and install**. The app verifies the installer against GitHub's SHA-256 digest before opening it. Close DCS before installing; your saved tuning is retained. If the check cannot reach GitHub, the status stays grey and you can retry or open Releases manually. Portable users can use the installer in their current folder or download the ZIP from Releases.

The Toolkit exposes a single motor command from 10–25, plus zero for stop. This app shapes those commands into cues; it does not send audio samples to the headset. Damage/ejection effects are unavailable. Aircraft signal quality, multiplayer restrictions and carrier behaviour need live-flight validation.

See [aircraft profiles and the flight QA checklist](docs/AIRCRAFT.md) for saved tuning, capability limits, catapult launches and descent-scaled touchdowns.

## Documentation

- [Setup and connection guide](docs/SETUP.md)
- [Effects and tuning](docs/EFFECTS.md)
- [Headset capabilities](docs/HARDWARE.md)
- [DCS signal reference](docs/TELEMETRY.md)
- [Shared-memory protocol](docs/PROTOCOL.md)
- [Testing and known limits](docs/TESTING.md)
- [Changelog](CHANGELOG.md)
- [Deferred features](docs/FUTURE_WORK.md)

## Build

Install Visual Studio 2026 C++ tools, CMake and vcpkg. Set `VCPKG_ROOT` to your vcpkg checkout:

```powershell
cmake --preset windows
cmake --build --preset release
ctest --preset release
```

Dependencies are pinned in `vcpkg.json`. For the installer and portable ZIP, install Inno Setup 6 and run `scripts/package.ps1`; use `-InnoCompiler` to select ISCC.exe if needed. No Sony firmware or Toolkit driver binaries are bundled.

## License and credits

GPL-3.0. See [third-party notices](THIRD_PARTY_NOTICES.md) and `licenses/` for upstream credits and licenses.

## Feedback and support

<p align="center">
  <a href="https://discord.gg/fs4WyaQPA"><img src="assets/support/social-discord.svg" width="38" height="38" alt="Discord" title="Discord"></a>
  <a href="https://www.reddit.com/r/AdamChsLab/"><img src="assets/support/social-reddit.svg" width="38" height="38" alt="Reddit" title="Reddit"></a>
  <a href="https://github.com/AdamChesters"><img src="assets/support/social-github.svg" width="38" height="38" alt="GitHub" title="GitHub"></a>
  <a href="https://www.linkedin.com/in/adamchesters/"><img src="assets/support/social-linkedin.svg" width="38" height="38" alt="LinkedIn" title="LinkedIn"></a>
  <a href="https://adamch.com"><img src="assets/support/social-website.svg" width="38" height="38" alt="AdamCh.com" title="AdamCh.com"></a>
</p>

<p align="center">Thanks for using dcs2psvr2-shaker. I like to fix bugs and implement features rapidly. Issue? Idea? Let's work on it! Contact me anywhere you prefer.</p>

<h3 align="center">Support</h3>

<p align="center">I love making things to have fun, share fun, and make life a lil better. If you got value out of one of them, feel free to buy us a coffee. Or a house. Push comes to shove, I prefer Aston Martin. Supporters get golden crowns on the Discord / tags on the subreddit!</p>

<p align="center">
  <a href="https://github.com/sponsors/AdamChesters"><img src="assets/support/support-github.svg" width="220" height="86" alt="GitHub Sponsors" title="GitHub Sponsors"></a>
  <a href="https://buymeacoffee.com/adamch"><img src="assets/support/support-buymeacoffee.svg" width="220" height="86" alt="Buy Me a Coffee" title="Buy Me a Coffee"></a>
  <a href="https://www.paypal.com/donate/?business=KLHSZPXTSVSAU&amp;no_recurring=0&amp;item_name=I%27ve+donated+to+lots+of+small+creators+for+their+useful+little+tools%2C+now+I+create+them.+Dig+one?+I%27d+love+your+support.&amp;currency_code=AUD"><img src="assets/support/support-paypal.svg" width="220" height="86" alt="Support with PayPal" title="Support with PayPal"></a>
</p>
