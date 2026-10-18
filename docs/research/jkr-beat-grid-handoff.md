# Handoff: jkr beat detection (continue fixing grid placement)

You are continuing work on **jkr**, DJNote's beat detection, in this Mixxx fork (`D:\Coding Projects\DJNote_Mixxx`, Windows, PowerShell plus Git Bash). Read `CLAUDE.md`, `GLOSSARY.md` (terms: jkr, Beat grid, Downbeat, Bar) and `docs/adr/0004-beat-grid-refined-on-the-kick.md` first.

## The user's report (start here)

The user tried jkr in the app: "it mostly works pretty good, but the grid is kinda not exactly where it's supposed to be". It needs "a little fix". **Before changing code, ask the user for specifics:**
- Is the grid early or late, and by roughly how much (a few ms, or a 16th or half a beat)?
- Is it every track or some? Which tracks, and what genre or format (FLAC, WAV, MP3)?
- Is it constant along the track, or does it drift toward the end (a BPM problem)?
- Is it the beat lines, or the bar/phrase lines (a downbeat problem)?
- Which view (scrolling waveform or overview, Phosphor or another waveform type)? What is it compared against: the visible kick transient, or Traktor?

Then reproduce it on those tracks with the benchmark below, and check whether it is analysis or display. The benchmark said jkr matches Traktor within ±1 ms on lossless files. A consistent visual offset could come from how the waveform or beat lines are drawn, not from the grid. It could also be the user's expectation differing from Traktor's convention, or the MP3/AAC decoder delay.

## State of the code

Branch `feature/beat-accuracy` (off `djnote`), **all uncommitted** (check `git status`):
- `src/analyzer/beatgridrefiner.{h,cpp}`: `mixxx::BeatGridRefiner`, the jkr grid placement.
- `src/analyzer/plugins/analyzerjkrbeats.h`: the jkr detector ids `jkr-qm-tempotracker` and `jkr-soundtouch`, plus `isJkr()` and `tempoPluginId()`.
- `src/analyzer/analyzerbeats.{h,cpp}`: `availablePlugins()` lists jkr QM (the default), jkr SoundTouch, then Mixxx QM and SoundTouch (unchanged). `usesGridRefiner()` means a jkr detector, plus the fixed tempo assumption or jkr SoundTouch. The refiner is fed in `processSamples`. In `storeResults` the Mixxx grid's BPM (`getLastMarkerBpm`) goes to `refine()`, which builds `Beats::fromConstTempo(firstBeat.toNearestFrameBoundary(), bpm, subVersion)`. jkr sub-versions carry `grid_refiner=1`: **bump it whenever placement changes**, so "re-analyze when settings change" picks it up.
- `src/test/beatgridrefinertest.cpp`: 6 synthetic-audio refiner tests and 4 `AnalyzerJkrBeatsTest` tests (jkr is the default, Mixxx QM output unchanged, both jkr options place the grid on the kick).
- `src/test/beatanalysisbench_test.cpp`: disabled benchmark, `BeatAnalysisBenchTest.DISABLED_Run`.
- `tools/beat_bench.py`: Traktor reference export and scoring.
- `CMakeLists.txt` (sources added), `GLOSSARY.md` (jkr, Downbeat), `docs/adr/0004-…`.
- Mixxx's QM pipeline was verified identical to before on 365 tracks. Keep it that way: only jkr may change.
- All 1069 unit tests pass.

## How BeatGridRefiner works (`refine(Bpm)`)

1. It keeps 1 ms envelopes (hop = round(sr × 0.001) frames) of the mono mix: a **low band** (4th-order Butterworth low-pass at 150 Hz) and a **high band** (2nd-order high-pass at 1 kHz).
2. **Onsets** are the half-wave-rectified 5 ms rise of log10 of each band's 5 ms mean energy, each normalized to mean 1, then summed.
3. **Tempo:** fold the onsets at candidate beat lengths and score by peakiness (variance / mean²). Search ±0.5% around the Mixxx BPM: a coarse pass on 4 ms data in 12 ms-drift steps, then a fine pass in 0.5 ms-drift steps.
4. **Tempo level:** try ×1.5 and ×2/3. Switch only if onset peakiness is higher **and** `lowSharpness` (the steepest rise of the smoothed folded low band, over its mean) is more than 1.15× higher. Both are needed: the low band alone gave false switches on house.
5. Below peakiness 0.05 → nullopt, and the Mixxx grid is kept (beatless material).
6. **Rounding:** `BeatUtils::roundBpmWithinRange(±0.05)`, kept if peakiness ≥ 0.9× the exact tempo's.
7. **Kick / phase (`findKickHops`):** fold the low band (smoothing 2) and log10 of the high band (smoothing 1). For each bin: attack = max of the high band over the next 10 hops minus its min over the previous 20; body = mean low band over the next 200 hops. kick = argmax(attack/max × body/max). The beat is the steepest high-band rise within ±15 hops of it (parabolic interpolation) **+ `kBeatOffsetSecs` = 1.5 ms**. That offset was calibrated so the median offset against Traktor is 0.
8. **Downbeat (`findDownbeatHops`):** per-beat log levels of both bands. Take the change from the previous beat, subtract each bar position's mean change (removes the kick/snare pattern), and sum the absolute residuals per position mod 4. The max is the downbeat. firstBeat = kick + downbeat × beat length, so it lies in [0, 4 beats).

## Lessons from tuning (don't repeat these)

- Low-band onsets for phase put the grid ~60 ms late. The kick's low end arrives after its click.
- Low-band-only kick selection picks bass 16ths in psytrance. The high-band attack works.
- A low-band-only tempo criterion broke house tracks (135 → 135.44, 136 → 203).
- "First kick" or "first sound" anchors are worse than downbeat detection. Psytrance intros run 16–64 beats before the kick enters.
- body² weighting fixed "Bumpy" but broke "Bae": that's overfitting.
- Known misses: Traktor factory house MP3s "Bumpin Flava - Bumpy" and "A Wee Dub" (off-beat; already wrong before jkr).

## Measuring (references = the user's Traktor 4 grids)

The Traktor collection is `C:\Users\Jakkaru\Documents\Native Instruments\Traktor 4.0.2\collection.nml`. The library is mostly psytrance (FLAC/WAV), plus some hip-hop and house MP3s.
- **Score phase on lossless only** (`--lossless`). Traktor doesn't trim the MP3 encoder/decoder delay (~−26 ms) or the AAC priming (+47.9 ms) the way Mixxx does.
- Bad references to ignore: "Ren - Meaning", "ren intro meaning", "New 2 2026-04-15", "BRAINDRAIN-REMEMBER INTRO", the `waveform-tests/Q*` test signals, and two AAC files Traktor lists twice with different BPMs.

Results on 334 lossless files:

| | Mixxx QM | jkr QM | jkr SoundTouch |
|---|---|---|---|
| Correct BPM | 88.6% | 98.2% | 94% |
| Within 2 ms over the whole track | 1.5% | 96.7% | 92.5% |
| Within 10 ms over the whole track | 69.5% | 98.2% | — |
| Bar anchor agrees with Traktor | 42% | 97% | — |
| Offset median / stdev | −4.9 ms / 78 ms | 0.0 ms / 0.75 ms | — |

The refinement costs about +20% of beat-analysis CPU.

Recreate the helpers in `$CLAUDE_JOB_DIR/tmp` (the old ones were deleted):

`buildtest.bat` (target: `mixxx-test` takes ~20 s incrementally; `mixxx` fails with LNK1104 if the dev app is running):
```bat
@ECHO OFF
SET "ROOT=D:\Coding Projects\DJNote_Mixxx\"
SET "BUILD_DIR=%ROOT%build\x64__portable"
SET "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
CALL "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >NUL || EXIT /B 1
CALL "%ROOT%tools\windows_buildenv.bat" setup >NUL || EXIT /B 1
cmake --build "%BUILD_DIR%" --parallel --target %1
EXIT /B %ERRORLEVEL%
```
Run it from Git Bash with `cmd //c "$(cygpath -w "$CLAUDE_JOB_DIR/tmp/buildtest.bat")" mixxx-test`.

`bench.sh <out-name> [list]` (~3 min for the whole library):
```bash
#!/bin/bash
T="$CLAUDE_JOB_DIR/tmp"; LIST="${2:-$T/list_music.txt}"
cd "D:/Coding Projects/DJNote_Mixxx/build/x64__portable"
export DJNOTE_BEATBENCH_LIST="$(cygpath -w "$LIST")" DJNOTE_BEATBENCH_OUT="$(cygpath -w "$T/$1.tsv")"
./mixxx-test.exe --gtest_also_run_disabled_tests --gtest_filter=BeatAnalysisBenchTest.DISABLED_Run > "$T/$1.log" 2>&1
tail -1 "$T/$1.log"
```
- `DJNOTE_BEATBENCH_PLUGIN` picks the detector for the "app" column (default `jkr-qm-tempotracker`; also `jkr-soundtouch`, `qm-tempotracker:0`, `mixxxbpmdetection`).
- `DJNOTE_BEATBENCH_THREADS` sets the thread count.
- The output TSV has columns `bpm_coarse`/`firstBeat_coarse` (Mixxx QM) and `bpm_app`/`firstBeat_app`.

Export the references, then score:
```bash
python tools/beat_bench.py refs "$USERPROFILE/Documents/Native Instruments/Traktor 4.0.2/collection.nml" "$T/refs.tsv" "$T/list.txt"
grep -v waveform-tests "$T/list.txt" > "$T/list_music.txt"
python tools/beat_bench.py score "$T/refs.tsv" "$T/<out>.tsv" --variant app --lossless --worst 30 [--csv "$T/scored.tsv"]
```
For a single-track investigation, put that track's path in a one-line list file. To inspect folded profiles, temporarily add a dump to `findKickHops` and print it as ASCII. That is how the kick/bass/hat structure was diagnosed. Remove the dump afterwards.

Run all tests with `build/x64__portable/mixxx-test.exe` (~1 min). Run the app with `.\djdev.bat run` from the repo root; it uses the separate profile in `build/dev-settings`. In that profile the user picks the detector in Preferences, then Beat Detection, and re-analyzes from the library context menu (Analyze, then Reanalyze).

## Conventions

- Commit only when the user asks.
- Issues live in the private repo `jakkarudev/DJNote_Mixxx_issues` (use `-R` with `gh`).
- Keep the ADR and glossary current. Write code comments in the style of the surrounding code.
