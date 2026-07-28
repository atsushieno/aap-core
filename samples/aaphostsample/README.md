# AAP host sample validator (AI slop)

The debug APK exposes a no-UI validator receiver for installed AAP plugin
packages. It binds a plugin service only for the duration of the request, so it
does not start a foreground service or request notification permission.

Build and install the validator host and a plugin APK:

```sh
./gradlew :samples:aaphostsample:assembleDebug
adb install -r samples/aaphostsample/build/outputs/apk/debug/aaphostsample-debug.apk
adb install -r /path/to/plugin-debug.apk
```

For the usual developer workflow, the helper installs the debug validator and
prints the JSON report. It accepts either an already-installed package or a
plugin APK (the latter needs `aapt` on `PATH`):

```sh
bash tools/aapval --package com.example.plugin
# or
bash tools/aapval --apk /path/to/plugin-debug.apk
```

`aapval` prints a short human report by default: the pass/fail/skip totals and,
for every failure, what was observed, why it matters, and a suggested fix. Use
`--verbose` to print every check or `--json` for the machine-readable report
used by CI. A failing validation exits with status 1.

Run the initial validation suite:

```sh
adb shell am broadcast --include-stopped-packages \
  -a org.androidaudioplugin.aaphostsample.VALIDATE \
  -n org.androidaudioplugin.aaphostsample.validator/org.androidaudioplugin.aaphostsample.AapValidatorReceiver \
  --es package com.example.plugin \
  --ei sample_rate 48000 \
  --ei frame_count 256 \
  --ei repeat_count 2
```

The broadcast result data is a JSON report. The suite verifies package
discovery, basic metadata consistency, parameter and MIDI event transport when
declared, finite audio output, state/preset operations when declared, and repeated
`connect -> instantiate -> prepare -> activate -> process -> deactivate -> destroy`
cycles. The receiver exists only in the debug APK; it is absent from release
builds.
