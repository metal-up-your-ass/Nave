# Nave — cabinet IR loader / convolution (guitar & bass)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Metal up your ass** symphonic-metal plugin suite (`github.com/metal-up-your-ass`).

## What this is
Nave is the "cabinet IR loader / convolution (guitar & bass)" member of the suite. AU / VST3 / Standalone, JUCE 8.

## Status (v0.1 — bootstrap complete)
Core DSP working, **22 Catch2 tests green**, CI (macOS + Windows, pluginval strictness 10 + auval) green. GUI is a functional v0.1 slider editor (custom LookAndFeel is roadmap M3). No signing yet (roadmap M4). Open work is tracked in this repo's GitHub **milestones/issues**.

## DSP
Nave is a cabinet IR loader built around juce::dsp::Convolution constructed with its default zero-latency, uniformly-partitioned configuration (Convolution::Latency{0}), chosen because reamping IRs are short and the workflow is latency-sensitive. Signal chain: Convolution (loaded IR or a default 1-sample unit-impulse) -> LoCut HPF -> HiCut LPF -> juce::dsp::DryWetMixer (delay-compensated, currently 0 samples) -> Level trim, matching the spec's stated order. LoCut's minimum (20 Hz) and HiCut's maximum (20 kHz) are treated as explicit "off"/bypass positions where the engine skips that filter's IIR processing entirely (not just an extreme cutoff) — this was necessary because even a 2nd-order Butterworth many octaves outside a test tone still imposes enough phase shift to fail a strict -80 dBFS sample-domain null; skipping it entirely gives a true bit-accurate passthrough at the default state. The IR file's absolute path is persisted as a plain ValueTree property on apvts.state (not an APVTS float parameter), round-tripping automatically through copyState()/replaceState(); file I/O only ever happens off the audio thread (editor FileChooser callback or setStateInformation, both message-thread/session-load contexts).

## Build & test
```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"      # shared JUCE 8.0.14 + Catch2 cache
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Tests Nave_Standalone --parallel 4
ctest --test-dir build --output-on-failure
```
Release/universal + pluginval + auval run in CI, not locally.

## Conventions & guardrails
- JUCE 8.0.14 via CPM · C++20 · AGPLv3 · Pamplejuce `SharedCode` pattern · manufacturer `Yvsv`, plugin code `Nave`, `com.yvesvogl.nave`.
- **Real-time safety:** no alloc/lock/file-IO/logging on the audio thread; allocate in `prepareToPlay`; `reset()` clears all state; `ScopedNoDenormals`; smoothed params; report latency via `setLatencySamples` where the chain adds any.
- **DryWetMixer gotcha (JUCE 8.0.14):** prime `setWetMixProportion(mix)` before `reset()` in `prepare()` (else it ramps from 100% wet). See sibling `overture`.
- **`main` is protected** — no direct commits; feature branch + PR, green CI required (Conventional Commits). New DSP needs tests (null/reference, NaN/Inf sweep, state round-trip, latency).

## Roadmap
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues. Read with `gh issue list --repo metal-up-your-ass/nave`.

## Suite context
Style references: sibling `metal-up-your-ass/overture` and `metal-up-your-ass/twist-your-guts`. The suite: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis, twist-your-guts.
