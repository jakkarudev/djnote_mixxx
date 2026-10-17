# The Phosphor scrolling waveform's data is computed at track load, not stored

The Phosphor scrolling waveform draws signed min..max and sample density from sample-level audio, which Mixxx's stored waveform (441 peak slices per second) does not contain. We decided to build its data (a mono sample buffer and the color track) from one full decode when a track is loaded into a deck, keep it in memory only, and change no stored analysis. This matches what Traktor appears to do (`WaveformDataGenerator`, no on-disk deck waveform cache), needs no library re-analysis, and leaves upstream's analysis files and versions alone.

## Considered Options

- **A new stored analysis format**: rejected. Sample-level shape data is about 42 MB per 8-minute track as raw int16. A histogram pyramid fine enough for the density at the deepest zoom is larger than the samples it summarises, so storing it would mean storing a degraded density, and every library track would need re-analysis.
- **A hybrid that stores only the color track**: rejected for the scrolling waveform. The shape still needs the full decode at load, and the color track comes from the same decode almost for free.

## Consequences

- Every deck load costs one background decode of the whole file (roughly 1–3 s for an 8-minute track), and the view fills in progressively, as Mixxx already does for unanalysed tracks.
- Memory per loaded track is about 42 MB for an 8-minute track, plus the color track.
- This ADR covers the scrolling waveform only. The Phosphor overview's data *is* stored, as a new analysis type written by the load pass and by library analysis: it is small (about 2000 columns per track) and has to show the whole track the moment a track loads. Its format is fixed after the overview is calibrated against Traktor's stored stripes.
