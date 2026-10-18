# jkr beat detectors re-place constant grids on the kick after Mixxx's beat tracker

DJNote adds two jkr beat detectors next to Mixxx's Queen Mary and SoundTouch detectors, which stay unchanged and selectable. With the fixed tempo assumption, a jkr detector uses Mixxx's detector only to propose a tempo. A separate stage, `BeatGridRefiner`, then picks the tempo level, the exact BPM, the beat phase and the first downbeat from 1 ms low-band (<150 Hz) and high-band (>1 kHz) envelopes of the whole track, folded at candidate beat lengths. We did this because measuring the tracker's grids against 300+ Traktor grids of our own library showed systematic faults it cannot fix from its 12 ms beats. Its grids were ~5 ms early, about 15% sat on the off-beat of rolling basslines, 8% of tracks (all psytrance) were detected at 2/3 tempo, and BPMs missed the integer by 0.01. Its first beat was also wherever the track started, so bar lines matched Traktor's downbeat on only 42% of tracks. With the refiner, 97% of grids are within 2 ms of Traktor over the whole track, and 97% share its downbeat.

The kick is found as the biggest high-band attack followed by low-band energy, not from the low band alone. In psytrance the rolling bass fills the low band, and the kick's low end arrives tens of ms after its click. The downbeat is the beat position, of the four in a bar, where band levels change the most beyond the change that repeats every bar. The constants, including a 1.5 ms offset to where Traktor places the beat, are calibrated against those Traktor grids with `tools/beat_bench.py` and the disabled `BeatAnalysisBenchTest`.

## Considered Options

- **Tune the Queen Mary tracker instead** (its detection function, step size or tempo prior): rejected. Its 512-frame hop limits beats to ±6 ms, and its tempo prior has no notion of kicks, so the off-beat and 2/3-tempo errors would remain.
- **Low-band onsets to place the beat**: rejected. They put the grid 60 ms late or on a bass 16th in most psytrance.
- **First kick (or first sound) of the track as the grid's anchor**: rejected. Psytrance intros run 16 to 64 beats before the kick enters, and the first kick often comes one beat after a kick-less downbeat.

## Consequences

- Mixxx's detectors were kept as they were, rather than improved in place, so users can compare and fall back to them, and their stored grids stay valid. jkr (Queen Mary tempo) is the default for new profiles.
- jkr grids carry `vamp_plugin_id=jkr-…` and `grid_refiner=1` in their sub-version. Tracks analyzed before are only re-analyzed when "Re-analyze when settings change" is on, or when the user re-analyzes them. Bump the value when the refiner places grids differently.
- With the fixed tempo assumption off, jkr (Queen Mary tempo) makes the same beat maps as Mixxx's Queen Mary.
- Tracks without a pronounced beat (spoken word, ambient) keep the plugin's grid.
- With jkr, bars and phrases (ADR 0001) start on a detected downbeat rather than on the first beat after the start of the track.
- Known misses: house tracks whose off-beat clap or bass is stronger than a kick-less low band can still land on the off-beat.
