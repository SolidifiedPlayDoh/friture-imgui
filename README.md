# friture-imgui

A C++ / [Dear ImGui](https://github.com/ocornut/imgui) rewrite of [Friture](https://github.com/tlecomte/friture),
the real-time audio analyzer, with a flat dark look.

![screenshot](docs/screenshot.png)

## Widgets

| Widget | What it does |
| --- | --- |
| Scope | Triggered time-domain view of one or two channels |
| FFT Spectrum | Smoothed spectrum, peak hold, A/B/C weighting, dual-channel difference, pitch label |
| 2D Spectrogram | Scrolling GPU spectrogram (Linear / Log / Mel / ERB / Octave axes) |
| Octave Spectrum | 1, 3, 6, 12 or 24 bands per octave with IEC-style time response |
| Long Time Levels | RMS level history up to 30 minutes |
| Delay Estimator | GCC-PHAT inter-channel delay (ms and metres) with correlation and polarity |
| Pitch Tracker | SWIPE-style pitch estimate with note names |
| Generator | Sine, white/pink noise, log sweep and burst on any output device |

Plus IEC peak / RMS level meters, record and playback of the last N seconds, free docking, and a persisted layout.

## Build

Needs CMake, Ninja and a C++17 compiler. Dependencies (GLFW, Dear ImGui, ImPlot, miniaudio, pocketfft)
are fetched by CMake.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/friture
```

Command-line flags: `--demo` (built-in test signal), `--list-devices`, `--no-save`,
`--screenshot out.png --frames N`, `--bench SECONDS`.

`FRITURE_CONFIG_DIR` overrides where settings and layout are stored.

## Credits and license

Derived from [Friture](https://github.com/tlecomte/friture) by Timothée Lecomte and contributors.
Licensed under the GNU GPL v3, like the original.
