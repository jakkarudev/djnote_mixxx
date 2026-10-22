# Key match tiers follow the DJNote app, not Mixxx's compatible keys

DJNote's library key highlighting (#83) judges tracks with the two tiers of the DJNote React app, not with Mixxx's `KeyUtils::getCompatibleKeys`. A strict match is the same key, ±1 Camelot hour in the same mode, or the relative major/minor. A fuzzy match is ±2 hours or ±1 semitone in the same mode. Diagonal moves (one hour and the other mode, e.g. 8A→9B) are not a match. The maintainer chose this because these are the relationships DJNote users already know from the app, and the fuzzy tier (energy and one-semitone moves) has no equivalent in Mixxx. Mixxx's `~key:` search keeps using `getCompatibleKeys` unchanged, so the search and the highlight deliberately disagree on the diagonals.

## Considered Options

- **Reuse `KeyUtils::getCompatibleKeys`**: rejected. It counts the diagonals, which the app rates as no match, and it has no fuzzy tier, so one colour would be all it could offer.
- **Change `~key:` to the DJNote tiers as well**: rejected. The feature is highlight only, and changing an upstream search would break CLAUDE.md's rule against repurposing upstream behaviour.

## Consequences

- The tier rules live in DJNote's own code. `getCompatibleKeys` must not be "fixed" to match them, and DJNote's code must not be switched over to `getCompatibleKeys`.
- A track that `~key:` finds may get no highlight (a diagonal), and a fuzzy-highlighted track is not found by `~key:`.
- Only the key relationships come from the app. Its key parsing, defaults and settings are not ported; keys are compared as Mixxx's `ChromaticKey` values.
