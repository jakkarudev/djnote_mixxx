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
- One colour scheme, "Nocturne", using its own graphics in `nocturne/` (`BtnScheme`/`KnobScheme`/`SliderScheme`/`StyleScheme` = `nocturne`).
  More schemes can be added to `skin.xml`.

## Nocturne base look (ticket #73)

- Font: Barlow Semi Condensed Regular + SemiBold (OFL, `res/fonts/`), set in `style.qss`; only Nocturne references it.
- Palette: scheme variables `ColorBg`, `ColorLine`, `ColorRaised`, `ColorRaisedDark`, `ColorOutline`, `ColorRing`, `ColorText`,
  `ColorTextDim`, `ColorTextBright`, `ColorBlue`, `ColorBlueDark`, `ColorDeckA`, `ColorDeckB` in `skin.xml`. QSS repeats the literals
  (see the "Nocturne base look" block at the end of `style_nocturne.qss`).
- Buttons are flat: `btn_embedded_*.svg` are 1px outlines (grey, blue when active); fills come from QSS. Text buttons: template
  `controls/button_flat_text.xml` with ObjectName `NocturneBtn` / `NocturneBtnFlat` / `NocturneBtnCell`.
- Knobs: thin ring (`nocturne/knobs/knob_bg_*.svg`) + pointer (`knob_indicator_<size>_<grey|blue>.svg`); the value arc is the skin's
  arc rendering, all arcs blue.
- Every selection and active state is `#448bf6` (selected rows/items use `#253b5b`). Red stays for recording/clip/error, orange for
  warnings/pending.
- Left as upstream: library sidebar feature icons (orange, from `res/images`), VU-meter PNGs.
- `skin_preview_Nocturne.png` is currently LateNight's PaleMoon preview; replace it with a capture of the finished skin.

## Header (ticket #74)

- Layout: NOCTURNE wordmark | main VU (two thin rows), CPU meter, clock, battery, REC, ON AIR, VINYL | SETTINGS, LIBRARY.
  A DECKS toggle appears next to the wordmark only while the library is maximised. LateNight's section toggles (waveforms, 4 decks,
  mixer, effects, samplers, mic/aux) are no longer in the header; they stay in the skin settings pane.
- VINYL toggles `[Nocturne],show_vinylcontrol` (the per-deck vinyl status panel).
- Dropped: fullscreen button (no Mixxx control exists for it; View > Full Screen is a menu action only).
- The battery widget hides itself on machines without a battery.

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
