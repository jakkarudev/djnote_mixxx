# NML import keeps one preparation per file and never merges

DJNote, like Mixxx, stores exactly one preparation (beat grid, key, cues, colour, rating) per library track, and a library track is one file path. When an NML import (#88) or a re-read reaches a file that DJNote has already prepared, DJNote keeps its own preparation by default. It takes the NML's only where the track has none, and decides that per part, all or nothing: grid and BPM, key, the cues (hotcues, main cue and loops together), colour and rating. The DJ can choose to replace DJNote's preparation with the NML's instead, and the dialog says this cannot be undone. A copied file in an export folder is a different file and therefore a different track, so it keeps the NML's preparation while the original keeps DJNote's. This is the one way both survive. An existing library track is used in place of such a copy only when every part the NML can carry is identical, and only after the DJ confirms. Traktor behaves the same way: its collection holds one entry per file path, including the copies its own import adds.

## Considered Options

- **Filling in slot by slot (each hotcue button on its own)**: rejected. It produces cue sets that neither DJNote nor Traktor made, such as DJNote's hotcues 1 and 4 next to Traktor's 2 and 7.
- **The NML always wins**: rejected. It would silently overwrite jkr grids and hotcues, and Mixxx has no undo for cue changes.
- **Keeping both cue sets on one file (the NML's hotcues on slots 9–16)**: rejected for the first cut. Most skins, Nocturne included, show only 8 hotcue buttons, so those cues would be invisible mid-set.
- **Always reusing the existing library track instead of a copy**: rejected. Any difference in preparation would then be lost on one side.

## Consequences

- Importing an export folder on the same PC can create duplicate library tracks, one per file, whenever the preparations differ. This is intended.
- A track that is missing at import gets a library row marked missing, with no preparation, because cue and grid positions need the file's sample rate. Re-reading the Source NML once the file is found again fills its preparation in through the same rule.
