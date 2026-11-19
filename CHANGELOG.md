# Changelog

## 0.1.3 Alpha

- PSVR2Toolkit: newer Toolkit releases, including v1.0.0-experimental-3, now work. The app no longer refuses Toolkit builds it has not seen before, so the "toolkit DLL build has not been validated" error is gone.
- Only v1.0.0-experimental-1 is refused, with a message to install a newer Toolkit release.
- Setup text now says to install the latest experimental PSVR2Toolkit release.

## 0.1.2 Alpha

- App icon: embedded image metadata removed. No visual change.

## 0.1.1 Alpha

- Feedback / Support: the feedback form now delivers messages. The delivery service was updated, so 0.1.0 can send feedback too.
- Feedback / Support panel checked against the shared support component 2.0.0.
- Build warning fix in feedback sending. No changes to haptics, aircraft profiles or settings.

## 0.1.0 Alpha

First release as dcs2psvr2-shaker.

- DCS World headset haptics for the F/A-18C Hornet, F-16C Viper, A-10C/II, F-14A/B Tomcat, F-4E Phantom and AH-64D Apache, with automatically selected per-aircraft tuning.
- Gear movement, gun bursts, descent-scaled touchdown, catapult launch, afterburner, store release, countermeasures, airborne buffet, gear/speedbrake airflow, and optional engine and runway ambience.
- Tests pane with adjustable pulses, sweeps and layered patterns on a shared six-second command graph.
- Setup checklist with headset USB, SteamVR, DCS and telemetry status lights.
- Feedback / Support panel with social links, support options, update check and a feedback form. Feedback sends only your name, email, message, app name and version.
- Per-user installer and portable ZIP, GitHub release checks and checksum-verified installer updates.
- Aircraft profiles and effects still need live-flight/headset testing. Command graphs do not measure physical vibration.
