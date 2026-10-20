# Nocturne skin notes

Nocturne is DJNote's Traktor-like skin. It is a copy of LateNight (see `docs/adr/0006-nocturne-copies-latenight.md`);
LateNight and the other upstream skins are not edited. Spec: issue #71 in the private issues repo.

## Dropped Traktor elements

Where Traktor shows something Mixxx has no control for, Nocturne drops it instead of faking it:

- Favourites bar of playlists
- Library status bar
- "Parallel" layout dropdown
- Phase meter above the overview
- CUE (move-cue) mode of the MOVE block
- Cue-type / AutoGrid dropdown
- Trash and MAP buttons
- The `<< < > >>` arrows in the strip under the decks
- Native Instruments branding (name, logo, artwork)

## Skin settings

- Every setting is stored as `[Nocturne],<name>`, with the same name LateNight uses after the comma. Nocturne reads and writes no
  `[Skin]` or `[LateNight]` key.
- Defaults (declared in the `skin.xml` manifest): shown = stacked waveforms, cover art (no spinnies), 8 hotcues, loop and beatjump,
  rate controls, key controls, crossfader, EQ knobs; hidden = effect rack, samplers, mic/aux, preview deck.
- Defaults only apply while the key is not yet saved in `mixxx.cfg`.
- One colour scheme, "Nocturne", still using PaleMoon's graphics (`palemoon/`). More schemes can be added to `skin.xml`.
- `skin_preview_Nocturne.png` is currently LateNight's PaleMoon preview; replace it with a capture of the finished skin.

## Known consequences of not using `[Skin]` keys

Some `[Skin]` controls are created and used by C++ and have no Nocturne equivalent:

- The View menu items (settings, microphones, vinyl control, preview decks, library cover art, maximized library) toggle `[Skin],...`
  and do nothing in Nocturne. Use the skin's own buttons.
- Auto DJ forces `[Skin],show_4decks` for decks 3/4 and does nothing in Nocturne.
- C++ derives `[Skin],show_spinny_or_cover`, `show_small_spinny_or_cover`, `show_big_spinny_or_cover` and `show_ducking_controls`.
  Nocturne rebuilds that logic in XML: `decks/spinny_cover_site.xml` (one container gated on `show_spinnies`, a sibling on
  `show_coverart` and not `show_spinnies`, wrapped by `select_big_spinny_or_cover` where needed),
  `helpers/skin_settings_cover_no_spinny_no_cover.xml`, and `[App],num_microphones` for the ducking controls.
- C++ still creates its `[Skin],...` controls and may save them in `mixxx.cfg`; Nocturne never touches them.
