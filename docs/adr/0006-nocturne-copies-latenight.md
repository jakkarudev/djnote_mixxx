# Nocturne is a full copy of LateNight with its own skin setting keys

Nocturne, DJNote's Traktor-like skin, lives in its own `res/skins/Nocturne/` directory, copied from LateNight and then restyled, and is built with skin XML, QSS, SVG and one bundled font only, with no C++ changes. Every skin setting is stored under a `[Nocturne],…` key that keeps LateNight's name after the comma (for example `[Nocturne],show_effectrack`). The copy means upstream's LateNight changes never conflict with Nocturne on a Mixxx update, and the separate keys let Nocturne start with Traktor-like defaults (effect rack, samplers and mic/aux hidden) without fighting LateNight over the shared `[Skin]` values.

## Considered Options

- **A third colour scheme inside LateNight**: rejected. Traktor's layout (flat text buttons, a compact centre mixer, a strip under the decks) needs different templates, not just different colours, and every edit would touch an upstream skin.
- **A new skin that includes LateNight's templates from its directory**: rejected. Upstream template changes would silently change Nocturne, and any template Nocturne needed to change would have to be copied anyway.
- **Reusing the shared `[Skin],show_…` keys**: rejected. Choices would carry over between skins, but Nocturne's defaults would overwrite LateNight's and the other way round.

## Consequences

- Fixes to LateNight from upstream are not picked up by Nocturne automatically; they have to be ported by hand when they matter.
- Renaming the `[Nocturne]` keys later would reset users' saved Nocturne skin settings.
- Nocturne is not the default skin; making it one needs a change to `src/skin/skinloader.cpp` and is a separate decision.
