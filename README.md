# Melt — psychedelic breakdown VST3

An effect that takes whatever you feed it and melts it into chaos: tape wow, pitch sagging,
buffer stutters, reversed grains, bitcrush, a wandering resonant filter, and a smeared
runaway-feedback wash. One **Chaos** knob drives everything; the other knobs set how much
each ingredient contributes once chaos rises.

## Controls

| Control | What it does |
|---|---|
| **Chaos** | Master amount. 0% = clean, 100% = full meltdown. Automate it for builds. |
| **MELT** button | Auto-ramps chaos from its current value to 100% over *Melt Time*; release to snap back (~0.6 s). Great on a MIDI-learned pad. |
| Warble | Tape wow & flutter (speeds up as chaos rises) |
| Dive | Pitch sags up to ~19 semitones and wanders; also makes stutters slow down like a tape stop |
| Stutter | Tempo-synced buffer repeats (1/4 down to 1/64 notes, smaller as chaos rises) |
| Reverse | Reversed grains, and reverse flips inside stutters |
| Crush | Bit depth and sample-rate reduction |
| Filter | Resonant low-pass that drifts randomly |
| Smear | Diffuse, cross-fed feedback delay that self-oscillates near the top (soft-limited) |
| Mix / Output | Dry-wet and output level. A soft limiter keeps peaks under 0 dBFS. |

Presets: Default Melt, Acid Wash, Tape Death, Glitch Storm, Reverse Dream, Bad Trip, Subtle Haze.
Stutters sync to the host tempo.

## Building on Windows

**Easiest — GitHub Actions:** push this folder to a GitHub repo. The included workflow
(`.github/workflows/build-windows.yml`) builds it; download `Melt-VST3-Windows` from the
run's *Artifacts* section.

**Locally:** install Visual Studio 2022 (with "Desktop development with C++") and CMake, then in this folder:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Melt_VST3
```

JUCE is downloaded automatically on first configure.

Copy `build\Melt_artefacts\Release\VST3\Melt.vst3` into `C:\Program Files\Common Files\VST3\`
and rescan plugins in your DAW. A standalone `Melt.exe` is also built for quick testing.

## Building on Mac

**GitHub Actions:** the included `build-mac.yml` workflow builds a universal (Apple Silicon + Intel)
VST3, Audio Unit and standalone app. Download `Melt-Mac` from the run's *Artifacts*.

**Locally on a Mac:** install Xcode and CMake, then:

```
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Melt_VST3 Melt_AU
```

Install: copy `Melt.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `Melt.component` to
`~/Library/Audio/Plug-Ins/Components/`. Builds aren't signed with a paid Apple certificate, so if
macOS blocks them, run in Terminal:
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Melt.vst3 ~/Library/Audio/Plug-Ins/Components/Melt.component`

## Files

- `Source/MeltEngine.h` — all the DSP, framework-free
- `Source/PluginProcessor.*` — parameters, presets, host tempo
- `Source/PluginEditor.*` — UI (the swirl behind Chaos melts as chaos rises)
- `test/render_test.cpp` — offline test: runs a drum loop through the engine and writes WAVs
