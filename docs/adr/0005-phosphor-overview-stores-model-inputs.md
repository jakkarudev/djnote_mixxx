# The Phosphor overview stores the model's inputs at fixed constants, not an image

The Phosphor overview's data is a new stored analysis type. Each track is split into 1920 columns of N = ceil(frames / 1920) samples, the same count as Mixxx's overview summary. Per column it stores the peak hold of the three band levels and a 62-row density profile across ±1.0 (65 bytes, zlib-compressed like Mixxx's waveform files). Color, brightness and every overview setting are applied when the overview image is drawn. The stored part is always computed with the fixed constants calibrated against Traktor's stripes (research §9), never with the scrolling waveform's tunable color track settings, so no preference change can make stored data stale. When a calibrated constant changes, the analysis version goes up and tracks are rebuilt.

A separate overview analyzer builds the data from the samples the analyzer thread already streams, wherever Mixxx's waveform analyzer runs (deck, sampler and preview loads, and library analysis), and only while Phosphor is the selected overview type. Switching to it rebuilds the tracks loaded in decks. Existing library tracks get the data the first time they are loaded or analysed, and no re-analysis of the library is forced.

## Considered Options

- **A rendered image, as Traktor's stripe**: rejected. Any color or brightness setting would need a re-analysis, and the image could not be redrawn at another height or with Normalize without losing the density.
- **A lean profile (min, max and 8 quantiles, 11 bytes per column)**: rejected. It is exact only for unimodal columns and loses the hollow and edge-bright shapes of tones and clipped masters, which are the point of the density.
- **Sharing the scrolling waveform's color track settings**: rejected. Changing a release time would quietly make every stored overview stale.
- **Building the data for every track, whichever overview type is selected**: rejected. It costs about 50 KB per track (about 500 MB for 10 000 tracks) for users who never pick Phosphor.

## Consequences

- About 122 KB raw per track before compression, 40–60 KB after.
- Switching the overview type to Phosphor shows existing tracks only after they are loaded or analysed; the overview fills in progressively, as Mixxx's does for unanalysed tracks.
- 62 rows match Traktor. With Normalize on, a quiet master is stretched (a −6 dB peak uses 31 rows) and looks softer.
