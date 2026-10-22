# NML export writes VERSION 20, the format of Traktor Pro 4

DJNote's NML export writes `<NML VERSION="20">` with `PROGRAM="Traktor Pro 4"`, copying the layout Traktor Pro 4.0.2 writes today for its collection, its History files and its playlist exports. Every grid marker is a `TYPE="4"` cue on `HOTCUE="-1"` with its own `<GRID BPM>`, so a grid whose tempo changes (Traktor 4's Flexible Beatgrid) goes across unchanged. Hotcues are `TYPE="0"` cues on their own buttons, and cue colours are written as `COLOR="#RRGGBB"`. Import reads both VERSION 19 and 20.

## Considered Options

- **VERSION 19, as in the maintainer's 2025 Traktor playlist export**: rejected. It has one tempo per track, so changing grids would have to be simplified to the first beat plus an average BPM. It puts the grid marker itself on hotcue button 1 and has no cue colours. A playlist export made with the installed Traktor Pro 4.0.2 on 2026-10-22 is VERSION 20, so the version 19 file must have come from an older Traktor.
- **Several markers in VERSION 19 without `GRID`**: rejected. Nothing shows Traktor reading per-marker tempos from such a file.

## Consequences

- Traktor 3 may not read DJNote's exports. Nobody using DJNote is expected to need Traktor 3.
- A future reader comparing our output with an old Traktor export will see a different version and a different grid-marker layout. That difference is deliberate.
