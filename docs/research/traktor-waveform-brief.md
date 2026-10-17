# Brief: a Traktor-style scrolling waveform for DJNote

Handoff for a design (grilling) session, written 2026-10-18 at the end of the research and prototype work. The goal is to decide how to build, in this Mixxx fork, a **scrolling waveform** that looks like Traktor Pro 4's Spectrum waveform. Glossary terms (`GLOSSARY.md`) are used as defined there: *scrolling waveform*, *overview*, *play marker*, *beat line*/*bar line*.

Read with:
- `docs/research/traktor-waveform.md`: the full evidence. §8 (test signals) and §8.5 (prototype calibration) supersede the earlier sections where they disagree. The "Revised plan" sits at the end of the Implications section.
- The prototype, on branch `prototype/traktor-waveform` (`tools/PROTOTYPE_traktor_waveform.py`, plus every screenshot and comparison sheet). The script is also left untracked in the working tree on `djnote`.
- `docs/adr/0001-bars-and-phrases-derived-from-beat-grid.md`: the only ADR so far.

## Status

- The user judged the prototype **"good enough"** after comparing it with Traktor on five real tracks (psytrance and trance: Gigando, ren intro meaning, Braindrain – Remember intro, Spiritual, Progress Paradox).
- One parameter set fits all five tracks with no per-track tuning.
- The look to build is settled. **How to build it in Mixxx is not**, and that is the subject of the session.

## The render model (what the prototype does)

All numbers are prototype defaults. "Measured" means it came from controlled tests or from fits across several screenshots. "Tuned" means it was set by eye and colour statistics on real-track screenshots.

### Shape (measured)
- **Signal:** the mono sum `m = (L+R)/2`. L = −R draws nothing.
- **Per column:** signed min..max of `m` over a window centred on the column. The window is ~2–3 columns wide and scales with zoom; the prototype uses a fixed 12 ms, which is ~1.6 columns at the default zoom.
- **Scale:** linear. Full scale is ≈ 0.8 × half the lane height, a fixed value. The waveform ignores gain, Auto-Gain and track loudness (in Traktor, gain does not change the waveform).
- **Polarity:** positive samples are drawn **below** the centre line.
- **No smoothing of the height:** onsets and offsets are sharp.
- **Zoom is set per beat:** 217.4 px per bar in all screenshots, so px/s scales with BPM.
- **Resolution:** at high zoom (513 px/s), individual 100 Hz cycles show as a zigzag. That needs sample-level data.

### Body brightness: sample density, the "phosphor" look (measured)
- **Hits:** for each pair of consecutive samples in the column's window, every row the line segment crosses gets weight `1/(rows crossed)`. The weights summed per row are its hits.
- **Density:** `density = hits / samples_in_window × lane_height/2`.
- **Pixel:** `clip(colour × 0.7 × density)`, per channel. Dense pixels turn lighter: orange goes to yellow, blue to cyan.
- **Consequences:** square waves look hollow, sines have bright edges, noise fills evenly, and bass shows "grain".

### Colour: a separate, smoothed, low-rate signal (partly measured, partly tuned)
1. **Filters.** Three zero-phase filters (R, G, B) are applied to `m`. Their magnitude responses are fitted to the stripe's pure-tone colour table (research doc §8.3): `g_c(f) = unit_table_c(f)^(1/0.35)`, interpolated on log-frequency.
2. **Levels.** Take the peak of each filter output per 1 ms block; the level is `peak^0.35`.
3. **Smoothing.** A one-pole follower per channel: attack 5 ms, release R 350 / G 180 / B 40 ms. Red is also max'ed with a backward follower (120 ms), so a kick's colour starts slightly before its onset.
4. **Saturation.** The unit colour is raised to the 5th power, keeping its length.
5. **Brightness.** Rescale so brightness follows `level^0.87`, measured from the Q3 level steps. Hue mixing uses 0.35 and brightness 0.87; one exponent for both blows out quiet passages to white.
6. **Channel gains** R/G/B = 1 / 0.85 / 1.8 (tuned).
7. **Per column,** the colour is sampled at the column centre.

### Background and silence
- Background is RGB 24.
- The prototype adds the density light to the background. The Q2a tones suggest Traktor draws covered pixels **opaque**, but additive scored better on music. This is unresolved, and the visual difference is small.
- Traktor draws digital silence as a 1-px black centre line and draws no axis line.

### How close it is
- **Outline:** correlation 0.91 with Traktor's.
- **Colour:** per-region mean colour within ~7–12 levels; pixel saturation and brightness match within ±0.03.
- **Remaining visible gap:** on dense tracks the centre line runs a bit yellow/white between kicks.
- **Colour mixing rule:** not found. On the Q9 two-tone tests, the green of a mix is close to the larger of the two tones' greens, not their sum. Every model tried is off by ~28 levels there, but it is good enough on music.

## What Mixxx has today (research doc §6)

- **Analysis:** 441 slices/s; each slice stores 4 bytes × 2 channels (low, mid and high band peaks plus the "all" peak), with `pow(0.632)` applied to high and all. There is no signed data, no sample-level data and no distribution (`src/analyzer/analyzerwaveform.*`, `src/waveform/waveform.h`).
- **Renderers:** under `src/waveform/renderers/allshader/` and `res/shaders/`. Heights follow `total_gain` and the EQ settings; some renderers apply an extra ×2; there is an axis line. The user runs Textured RGB (type 23) with visual gain 1.5.
- **The gap:** the shape and density need **sample-level (or near) data**, which Mixxx's waveform does not keep. The colour track could start from Mixxx's existing band data but would not match its filters.

## Decisions for the session

1. **Where the shape/density data comes from.**
   - (a) Compute from decoded audio at track load, as Traktor appears to (`WaveformDataGenerator`). There is no re-analysis, but it costs memory, CPU and load latency per deck.
   - (b) A new analyzer waveform version stored in the library. That needs re-analysis and a schema/format change, and diverges from upstream.
   - (c) A hybrid of the two.
2. **The shape representation.**
   - Raw mono samples (an 8-min track is ~42 MB as int16 per deck) against a multi-resolution min/max + small amplitude-histogram pyramid.
   - The zoom range it must support and how faithful the density must stay at each zoom.
   - How the window scales with zoom.
3. **The colour pipeline.**
   - Offline zero-phase FFT filtering (whole track, or chunked overlap-add) against causal IIR filters.
   - Whether to reuse Mixxx's low/mid/high bands at first.
   - The storage rate for the colour track (1 ms blocks × 3 bytes ≈ 1.4 MB per 8 min).
   - Whether colour is computed in the same pass as the shape.
4. **The renderer.**
   - A new allshader renderer and `WaveformType`, or changes to an existing one.
   - Where density is computed: on the CPU per column with a texture upload, or in a shader.
   - The frame budget for up to 4 decks at 60 fps. Behaviour when scrolling and scratching (reverse playback) and on zoom change.
5. **Mixxx features that interact with it.**
   - Gain and ReplayGain (Traktor ignores both), and EQ/kill visualisation (Traktor's behaviour is unknown).
   - Stems (Traktor draws each stem separately), loops, slip mode.
   - Beat, bar and phrase lines, and the play marker.
   - Whether the skin's colours or the "All" visual gain apply.
6. **Settings.** Which parameters to expose (saturation, channel gains, brightness, the opaque/add choice, height), whether to offer presets like Traktor's colour modes, and the defaults.
7. **Scope.** Is the **overview** in scope? Traktor's stripe is a stored 1024 × 64 image. Mixxx's overview could reuse the new colour track. Is the user's Textured RGB setup kept as an option?
8. **Verification.** Use the Python prototype as the reference implementation, for example with golden images for the test signals (WAVs from `tools/waveform_test_signals.py`) and a few tracks. Fix the tolerances and decide where those tests live.
9. **Glossary and ADRs.**
   - New terms likely needed: e.g. "density rendering", "colour track", the renderer's user-facing name.
   - At least one ADR is likely, on the data source in decision 1.

## Open research questions (not blocking)

- The exact two-colour mixing rule (Q9 data is in research doc §8.5).
- Whether the shape window depends on zoom: only two zooms have been checked.
- Opaque vs additive drawing.
- Whether Traktor's waveform follows EQ/kills.
- Q1: does the stripe hold band data or a rendered image?

## Reproducing the prototype

- Needs `uv` and `ffmpeg`. Run from the repo root:
  - `git show prototype/traktor-waveform:tools/PROTOTYPE_traktor_waveform.py > tools/PROTOTYPE_traktor_waveform.py` (if it is not already in the working tree)
  - `uv run tools/PROTOTYPE_traktor_waveform.py "G:\Music\DESKTOP MUSIC DJ SETS\UPDATED TRACKLIST\01 - Gigando.flac" --compare "SCREENSHOTS/waveforms/Screenshot 2026-10-17 203136.png"`
- Other tracks need `--time` (the track time under the play marker; fit values are in the branch's commit message) and `--px-per-sec` (217.4 × BPM / 240).
- Test signals: `python tools/waveform_test_signals.py` writes Q2–Q9 into `waveform-tests/`.
