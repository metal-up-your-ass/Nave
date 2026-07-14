# Nave

*Where the sound resonates — impulse-response cabinet simulation for guitar and bass DI.*

[![CI](https://github.com/metal-up-your-ass/nave/actions/workflows/ci.yml/badge.svg)](https://github.com/metal-up-your-ass/nave/actions/workflows/ci.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

> **Work in progress.** Nave is pre-1.0 and under active development. There are no built binaries or releases yet — building from source is currently the only way to run it. Expect breaking changes until v1.0.0 ships (see [Roadmap](#roadmap)).

<!-- ==BEGIN BODY== (plugin engineer: replace this block with What it is / Features / Signal flow / Roadmap) -->
## What it is

Nave is a cabinet impulse-response (IR) loader built on JUCE 8, aimed at reamping guitar and bass DI tracks: load a cab (or full-rig) IR captured from a real speaker/mic setup and Nave convolves it with your DI signal using a zero-latency partitioned convolution engine, then shapes the result with a pair of post-convolution filters, a dry/wet mix, and an output trim. With no IR loaded, Nave runs a unit-impulse (delta) IR - mathematically a passthrough - so it is a valid, transparent effect straight out of the box.

## Features (v0.1 scope)

- **IR loading** - load any WAV/AIFF impulse response via a file chooser; loading happens off the audio thread and never blocks or allocates during playback
- **Zero-latency convolution** - `juce::dsp::Convolution`'s zero-latency uniformly partitioned algorithm, so Nave never adds plugin delay compensation overhead
- **LoCut** - post-convolution high-pass, 20 Hz - 800 Hz (default 20 Hz, an explicit "off"/bypassed position), removes low-end mud
- **HiCut** - post-convolution low-pass, 2 kHz - 20 kHz (default 20 kHz, also an explicit "off" position), tames fizz
- **Mix** - dry/wet, default 100% (fully wet) - a cabinet IR is normally run fully in the signal path
- **Level** - output trim, -24 dB to +24 dB
- Full state save/recall via `AudioProcessorValueTreeState`, including the loaded IR's file path

## Signal flow

```
Input --> Convolution (loaded IR or default delta) --> LoCut (HPF, 20-800 Hz) --> HiCut (LPF, 2-20 kHz)
                                                                                          |
                                    Output <-- Level (output trim) <-- Mix <--------------+
                                                                          ^
                                                                          |
                                                              delay-compensated dry path
```

See [`docs/architecture.md`](docs/architecture.md) for the full breakdown, including the convolution/latency strategy, the filter-bypass-at-range-extremes design, and IR file state handling.

## Roadmap

| Milestone | Description | Status |
|---|---|---|
| M0 | Bootstrap - project skeleton, CI, docs | Done |
| M1 | DSP core - IR loading, convolution, LoCut/HiCut/Mix/Level signal path, latency reporting, unit tests | Done |
| M2 | Custom GUI | Planned |
| M3 | Release engineering - signing, notarization, installers, v1.0.0 | Planned |
<!-- ==END BODY== -->

## Installation

No pre-built binaries are published yet (see the work-in-progress notice above). Once releases begin, installation will follow the standard plugin locations:

**macOS**

| Format | Path |
|---|---|
| AU (Component) | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

If Logic Pro doesn't pick up the plugin after installing, force a rescan by resetting the AU cache:

```sh
killall -9 AudioComponentRegistrar
auval -a
```

**Windows**

| Format | Path |
|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\` |

## Building from source

Requires JUCE 8.0.14, C++20, and CMake ≥ 3.24. See [`docs/building.md`](docs/building.md) for full prerequisites and step-by-step build/test commands for macOS and Windows.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

Nave is licensed under the [GNU Affero General Public License v3.0](LICENSE) (AGPLv3).

This project uses [JUCE](https://juce.com) 8, whose open-source tier is licensed under AGPLv3 (as of JUCE 8; JUCE 7 and earlier used GPLv3), which is why this project is AGPLv3 rather than GPLv3. See [`docs/adr/0002-agplv3-licensing.md`](docs/adr/0002-agplv3-licensing.md) for the full reasoning.

VST is a registered trademark of Steinberg Media Technologies GmbH.

Nave is an independent open-source project and is not affiliated with, endorsed by, or sponsored by any plugin manufacturer.
