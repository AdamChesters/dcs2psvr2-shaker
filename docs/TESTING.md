# Testing and limitations

Build on Windows and run `ctest --test-dir build --output-on-failure` with the
appropriate build configuration. The test targets present in each revision cover
haptic scheduling, profiles, DCS bridge behavior, host control and update validation.
Use the test Toolkit substitute for automation; do not treat it as headset testing.

Validate effects on the supported aircraft and headset before relying on them.
Motor acknowledgements do not measure physical vibration or guarantee a hardware
stop. Flight conditions, carrier deck motion, export restrictions, connection loss
and driver faults can change behavior. Use conservative strength and the available
mute controls. See [hardware limits](HARDWARE.md) and [setup](SETUP.md).

Installer compilation and automated tests do not certify interactive updates or
saved-setting preservation on every Windows installation. Back up user settings.
