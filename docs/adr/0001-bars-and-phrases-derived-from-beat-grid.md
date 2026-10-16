# Bars and phrases are derived from the beat grid, assuming 4/4

DJNote's bar lines, phrase lines and bar-unit hover readout need bar and phrase boundaries. Mixxx's `Beats` has no time signature, downbeat or phrase data. We decided that a bar is always 4 grid beats and a phrase is N bars (a preference, 8 by default), both counted from the first beat of the track's beat grid. We store nothing new per track. The analyzer already places the grid at the first downbeat, and users can fix a misplaced grid with the existing grid-shift controls. Those controls fix the bar and phrase lines too.

## Considered Options

- **Anchor at the main cue point**: rejected. The cue moves often during preparation, and every move would shift all phrase lines.
- **A per-track phrase anchor or time signature stored in the library**: rejected for now. It needs a database schema change and its own editing UI, and it would diverge from upstream Mixxx's track model.

## Consequences

- Tracks not in 4/4, or with a pickup before the first downbeat that the analyzer placed wrongly, will have misaligned bar and phrase lines. The fix is to shift the beat grid, not to edit phrases.
- If downbeat or phrase data is ever stored, existing users' bar and phrase lines may move. Supersede this ADR when that happens.
