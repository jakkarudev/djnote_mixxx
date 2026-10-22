# NML export writes VERSION 20, the format of Traktor Pro 4

DJNote's NML export writes `<NML VERSION="20">` with `PROGRAM="Traktor Pro 4"`, copying the layout Traktor Pro 4.0.2 writes today for its collection, its History files and its playlist exports. Every grid marker is a `TYPE="4"` cue on `HOTCUE="-1"` with its own `<GRID BPM>`, so a grid whose tempo changes (Traktor 4's Flexible Beatgrid) goes across unchanged. Hotcues are `TYPE="0"` cues on their own buttons, and cue colours are written as `COLOR="#RRGGBB"`. Import reads both VERSION 19 and 20.

## Considered Options

- **VERSION 19, as in the maintainer's 2025 NML export from Traktor**: rejected. It has one tempo per track, so changing grids would have to be simplified to the first beat plus an average BPM. It puts the grid marker itself on hotcue button 1 and has no cue colours. A playlist export made with the installed Traktor Pro 4.0.2 on 2026-10-22 is VERSION 20, so the version 19 file must have come from an older Traktor.
- **Several markers in VERSION 19 without `GRID`**: rejected. Nothing shows Traktor reading per-marker tempos from such a file.

## Export rules (audit of 2026-10-22, #116)

Checked against 452 real Traktor entries. A constant grid writes one marker at its stored anchor (the first position at or after 0 that is a whole number of bars from it, when the anchor is negative), not at the track's first beat. A flexible grid writes one marker per tempo change, each with the BPM of its whole section; only pieces that differ by at most one frame per beat, which is what the database's integer-frame beats produce, and the closing marker the import appends, are merged. `COLOR` is omitted on a cue with DJNote's default cue colour (the one cues imported without a colour get) and written for every other colour. `INFO@KEY` is always Open Key (`6m`), `FLAGS` is `28` on a locked track (Traktor's lock bit 0x10) and `12` otherwise. A grid built only from file tags (empty beats sub-version, BPM not locked) is not exported as markers; only `TEMPO@BPM` is written, as Traktor does for tracks without a grid.

## Consequences

- Traktor 3 may not read DJNote's exports. Nobody using DJNote is expected to need Traktor 3.
- A future reader comparing our output with an old Traktor export will see a different version and a different grid-marker layout. That difference is deliberate.
