# Nocturne skin notes

Nocturne is DJNote's Traktor-like skin. It is a copy of LateNight (see `docs/adr/0006-nocturne-copies-latenight.md`);
LateNight and the other upstream skins are not edited. Spec: issue #71 in the private issues repo.

## Library (#80)
- Layout is LateNight's: search box above the sidebar (left), track table on the right. Preview deck stays hidden by default; favourites bar and status bar do not exist.
- Row height is the library setting `[Library],RowHeight` (Preferences > Library); QSS cannot set it (Mixxx applies it as the section size). Set it to about 40 px for cover-art-tall rows as in Traktor. Which columns show and their widths are library settings too.
- Column header text cannot be uppercased (QSS has no `text-transform`; text comes from C++). Headers are small and grey instead.
- Sidebar item icons are set in C++ and keep their own colours.

## Dropped Traktor elements

Where Traktor shows something Mixxx has no control for, Nocturne drops it instead of faking it:

- Favourites bar of playlists
- Jump-to-start (|<) button left of the overview
- Deck buttons with no Traktor counterpart on the full deck: eject, star rating, quantize, repeat, cue-position display, hotcue shift / undo / BPM-lock buttons (the controls still exist; use the library, shortcuts or the compact deck)
- Per-deck vinyl status panel is only shown by the VINYL header toggle (in the SYNC row), not as always-visible buttons
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
- `skin_preview_Nocturne.png` is a 1920x1080 capture of the finished skin (two tracks loaded, Phosphor waveforms).

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

## Strip (#79)
- 2 decks: crossfader, headphone MIX/VOL and main gain live in mixer/nocturne_strip.xml (row between decks and library, skin.xml DecksMixerContainer); mic/aux rack is the strip left part (hidden by default). 4 decks: strip hidden, old mixer crossfader + main/headphone column and the mic/aux row as before.
- Dropped in the 2-deck mixer: per-channel crossfader assign switches, quick-effect preset selectors, headphone split button, booth gain, balance, main/headphone FX assign buttons.

## Final pass (#81)
- Screen sizes: the smallest reachable window is 1296x729 (skin MinimumSize 1280x668 plus Windows chrome; 1280x720 itself cannot be set). Mixxx skins have no
  width media queries, so there is no automatic compact fallback: the default layout is made to fit by shrinking instead. Transport, loop and IN/OUT buttons are
  fixed 30-34 px wide, loop-size cells shrink (21 px min), hotcue slots and the library flex. Nothing is clipped or overlaps at 1296x729. Lanes are ~45 px each there.
  Use the Hotcues 4/8 setting or the Loop/Beatjump settings to save more room.
- Settings: every setting in the pane was toggled once and works. Rate Adjust Buttons, Key Controls (full deck) and Hotcue Shift Buttons were removed from the pane because the
  controls they gated no longer exist on Nocturne's full deck (rate fader is now gated by Rate Controls, the KEY knob and note icon are always shown). Compact (mixer hidden)
  and mini decks keep LateNight's layout and old graphic buttons; they are readable but not restyled.
- Intro/outro cue buttons are flat text: IS, IE (intro start/end), OS, OE (outro start/end); left click sets/jumps, right click clears, blue while set.
- VINYL toggle: shows VINYL status, ABS/REL/CONST, CUE/HOT and PASS next to MASTER on each full deck.
- Hotcue right-click opens Mixxx's cue menu (colour, label, delete); shift + right-click clears directly.
- Known visual limits: library row height/thumbnail size is a Library preference; the 4-deck mixer and effect/sampler rows keep LateNight's controls (dark but dimmer); the time block shows
  elapsed (+ remaining when the global time mode is "both") over track length.
- Skin switching: Nocturne stores `[Nocturne],*`, LateNight `[Skin],*`; switching skins in `mixxx.cfg` and back left both untouched (checked).
