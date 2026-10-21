# Nocturne2 skin notes

Nocturne2 is a full copy of Nocturne restyled to sit closer to a Traktor Pro 4 capture. Nocturne itself is unchanged.
Same rules as Nocturne: skin XML, QSS and SVG only, no C++ changes, nothing Mixxx has no control for.

## What differs from Nocturne
- Settings are stored as `[Nocturne2],<name>` (Nocturne's `[Nocturne],<name>` with the group renamed), so both skins keep
  their own choices. Template paths are `skins:Nocturne2/...`. The colour scheme is still called "Nocturne", so the preview
  file stays `skin_preview_Nocturne.png` (Mixxx names previews after the scheme).
- Font: Open Sans Regular (bundled with Mixxx) instead of Barlow Semi Condensed; Traktor's text is a regular-width sans.
- Lower deck panel (transport, hotcues, MOVE/GRID) is Traktor's grey `#262626` band, running down to the line under the decks. Play and reloop are
  outlined cells with drawn glyphs (`nocturne/buttons/nocturne_play_*.svg`, `nocturne_loop_*.svg`); loop sizes and IN | OUT are
  outlined groups split by 1px lines. The selected loop size is a grey fill, as in Traktor; blue only while that loop plays.
- Separator lines are Traktor's 2 px black: the waveform/deck splitter handle (no grip), either side of the mixer (deck
  borders), under the deck/mixer band and under the strip (`#NocturneStrip` top/bottom). Decks and mixer have no margins, so
  the lines join and the mixer sits flush between them.
- Hotcue slots: flat `#323232` bars with 2px gaps. Set slots still show the cue colour (WHotcueButton paints it; QSS cannot override).
- SYNC / MASTER and tempo bend: borderless `#262626` cells on a `#1f1f1f` row.
- Knobs: solid `#3c3c3c` disc + light pointer (`nocturne/knobs/*.svg`); the grey open ring is the knob's `ArcBgThickness` /
  `ArcBgColor` (skin.xml `ArcBg*`), drawn by Mixxx under the 3px blue value arc.
- 2-deck mixer (`mixer/mixer_2decks.xml` + `nocturne_channel_gain_col.xml`, `nocturne_channel_vu.xml`, `nocturne_channel_eq_col.xml`,
  `nocturne_knob.xml`, `nocturne_eq_knob.xml`) is rebuilt on Traktor's measured grid, 248 px wide:
  outer column 60 | VU 8 | EQ column 56 | 2 px black centre line | mirrored. Knob rows every 55 px, captions 24 px below the
  knob centre, an 8 px dot right of each caption (clip light, quick effect on, EQ kills, key lock; EQ kill dots now default on).
  Mixer knobs are 35x35 with ring r 16 / 3 px, disc r 11.5; ring, disc and pointer share the widget centre (no centre offset).
  GAIN / FLTR / KEY arcs start at the centre (ArcUnipolar false).
- FLTR caption is Mixxx's quick-effect chain picker (EffectChainPresetSelector) styled as Traktor's "v FLTR" pill; it shows the
  chain preset's name ("Filter" by default).
- Channel VU: Traktor's segmented 4x165 meter (33 segments, the top 9 above 0 dB), PNGs because the GL VU meter draws SVGs black.
- Faders: every groove and cap SVG is exactly the size of its widget and drawn with `scalemode="STRETCH"`, so nothing is rescaled
  off-centre. Channel fader 34x52 groove + 34x13 cap, level bar on the track centre with `BarMargins` = half the cap (6.5) so it
  ends at the cap centre; the groove paints the last 7 px blue so the line reaches the bottom as in Traktor. Rate fader 34x52 with
  Traktor's tick pattern and the same cap, its bottom level with the middle of the overview, 29 px in from the deck edge.
  Crossfader 170x32 black track + 11x32 cap, grey A / B labels.
- Headphone cue (PFL): Traktor shows none in the mixer, so the two cue buttons sit either side of the crossfader in the strip.
- Decks and mixer butt together, split by 2px black lines (no raised LateNight borders). Deck height cap raised 230 -> 236 px.
- Library: Open Sans 15px rows, thin column separators via `::item` borders (the table grid is off in C++). The library cover
  art box under the sidebar defaults to hidden (Traktor has none).
- All QSS changes are the "Nocturne2: closer Traktor pass" block at the end of `style_nocturne.qss`, which overrides the blocks above it.
- Checked at 1920x1080 and at the 1296x729 minimum: nothing clipped; at the minimum the loop-size cells are tight but readable.

# Nocturne skin notes (inherited)

Nocturne is DJNote's Traktor-like skin. It is a copy of LateNight (see `docs/adr/0006-nocturne-copies-latenight.md`);
LateNight and the other upstream skins are not edited. Spec: issue #71 in the private issues repo.
In Nocturne2 every `[Nocturne],...` key below is `[Nocturne2],...`.

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

- Every setting is stored as `[Nocturne2],<name>`, with the same name LateNight uses after the comma. Nocturne reads and writes no
  `[Skin]` or `[LateNight]` key.
- Defaults (declared in the `skin.xml` manifest): shown = stacked waveforms, cover art (no spinnies), 8 hotcues, loop and beatjump,
  rate controls, key controls, crossfader, EQ knobs; hidden = effect rack, samplers, mic/aux, preview deck.
- Defaults only apply while the key is not yet saved in `mixxx.cfg`.
- One colour scheme, "Nocturne", using its own graphics in `nocturne/` (`BtnScheme`/`KnobScheme`/`SliderScheme`/`StyleScheme` = `nocturne`).
  More schemes can be added to `skin.xml`.

## Nocturne base look (ticket #73)

- Font: Barlow Semi Condensed Regular + SemiBold (OFL, `res/fonts/`), set in `style.qss`; only Nocturne references it.
- Palette: scheme variables `ColorBg`, `ColorRaisedDark`, `ColorOutline`, `ColorText`, `ColorTextDim`, `ColorTextBright`, `ColorBlue`,
  `ColorDeckB` in `skin.xml`; the XML colour variables (overview/waveform background, beat/axes/cue colours, knob arcs, slider bars)
  are built from them. QSS and the SVG graphics cannot read skin variables and repeat the same hex literals (see the "Nocturne base look"
  block at the end of `style_nocturne.qss`), so a palette change has to be made in the manifest, the QSS and the SVGs.
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
- VINYL toggles `[Nocturne2],show_vinylcontrol` (the per-deck vinyl status panel).
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
- Settings: every setting in the pane was toggled once and works. Hotcue Shift Buttons was removed from the pane because the
  controls it gated no longer exist on Nocturne's full deck. Rate Adjust Buttons gates the tempo-bend arrows (`show_rate_control_buttons`), Key Controls gates the
  mixer KEY knob (`show_key_controls`); both default to shown. The rate fader is gated by Rate Controls. Compact (mixer hidden)
  and mini decks keep LateNight's layout and old graphic buttons; they are readable but not restyled.
- Intro/outro cue buttons are flat text: IS, IE (intro start/end), OS, OE (outro start/end); left click sets/jumps, right click clears, blue while set.
- VINYL toggle: shows VINYL status, ABS/REL/CONST, CUE/HOT and PASS next to MASTER on each full deck.
- Hotcue right-click opens Mixxx's cue menu (colour, label, delete); shift + right-click clears directly.
- Known visual limits: library row height/thumbnail size is a Library preference; the 4-deck mixer and effect/sampler rows keep LateNight's controls (dark but dimmer); the time block shows
  elapsed (+ remaining when the global time mode is "both") over track length.
- Loop size cells: a click toggles that loop, blue fill = that loop is active. The selected size (`beatloop_size`) is shown by blue text and border (a
  `highlight` property bound with an `IsEqual` transform), also while no loop plays.
- Intro/outro cues and EQ kill dots default to hidden. Dropped: GRID SET (`beats_translate_curpos`) and the key lock dot under the mixer KEY knob (the deck's key lock button stays).
- Skin switching: Nocturne stores `[Nocturne2],*`, LateNight `[Skin],*`; switching skins in `mixxx.cfg` and back left both untouched (checked).
