# JKR-Parallel: Traktor elements mapped to Mixxx skin widgets and controls

Reference for spec #22 (JKR-Parallel skin). It lists every element of Traktor Pro 4's two-deck Parallel screen, as in the user's screenshots in `SCREENSHOTS/traktor_screenshots/`, with the Mixxx skin widget and `[Group],control` that builds it, or why it needs C++. Read this instead of re-exploring the skin parser.

Widget tags are registered in `src/skin/legacy/legacyskinparser.cpp:529-633`.

**Status:**
- **skin**: buildable with existing widgets, controls, QSS and SVG.
- **partial**: works, with the limits noted.
- **C++**: needs new code. The spec's ticket covers it.

## Header bar

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Wordmark (replaced by Mixxx logo) | `WidgetGroup` or `Label` with an SVG background (`res/images/mixxx_logo.svg`) | skin | |
| CPU meter (dotted horizontal) | `VuMeter` on `[App],audio_latency_usage` plus `StatusLight` on `[App],audio_latency_overload` | skin | LateNight does this in `res/skins/LateNight/toolbar.xml:164-169`. Use a dotted `PathVu` pixmap |
| Clock | `Time` (`ShowSeconds`, custom format) | skin | `src/widget/wtime.cpp:34` |
| Battery | `Battery` (`PixmapsCharging` / `PixmapsDischarging`) | skin | |
| Record dot | `PushButton` on `[Recording],toggle_recording`, lit from `[Recording],status` | skin | |
| Layout dropdown | `ComboBox` (`<State><Number><Text>`) on a skin attribute such as `[Skin],jp_layout` | skin | Manifest `<attribute config_key=… persist="true">` creates the control (`legacyskinparser.cpp:293-305, 346-399`) |
| Maximise library icon | `PushButton` on `[Skin],show_maximized_library` | skin | |
| Auto DJ icon | `PushButton` on `[AutoDJ],enabled` | skin | |
| Preferences and fullscreen icons | none | C++ | Only menu actions exist, no controls (`src/mixxxmainwindow.cpp`) |
| Controller, audio and speaker status icons | `StatusLight` (e.g. on `[App],audio_latency_overload`) | partial | No control opens a Preferences page |

## Parallel waveforms

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Stacked scrolling waveforms | 2× `Visual` (`[Channel1]`, `[Channel2]`) in a vertical `WidgetGroup` | skin | See LateNight's `waveforms_container.xml` |
| Deck letter A/B | `Label` in a column to the left of each `Visual` | partial | Can't be drawn over the waveform. GL waveforms are native window containers (`src/widget/wglwidgetqopengl.cpp:42`), so `<Layout>stacked</Layout>` (`src/widget/wwidgetgroup.cpp:129-131`) won't overlay them |
| Shared play marker | each `Visual` draws its own; its position is a global preference | partial | The markers line up if both widgets have the same width |
| Waveform type | global preference | partial | A skin can't choose it (see "Facts" below) |

## Deck header

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Cover art | `CoverArt` with `<Group>[ChannelN]</Group>` | skin | |
| Title / artist / album | `TrackProperty` with `title` / `artist` / `album` | skin | |
| Record label, right-aligned | none | C++ | `Track` has no label property (`src/track/track.h:58-86`) and there is no database column. The label is parsed into `AlbumInfo` only. See ADR-0005 |
| Elapsed above remaining | `NumberPos` `[ChannelN]` | partial | The mode comes from the global `[Controls],ShowDurationRemaining`; value 2 shows both on one line (`src/widget/wnumberpos.cpp:111-118`) |
| BPM | `NumberBpm` or `Number` on `[ChannelN],visual_bpm` | skin | |
| Tempo % | `NumberRate` `[ChannelN]` | skin | |
| Big deck letter | `Label` | skin | |
| Tempo fader | `SliderComposed` on `[ChannelN],rate` | skin | |
| SYNC / MASTER | `PushButton` on `[ChannelN],sync_enabled` / `[ChannelN],sync_leader` | skin | |
| Phase meter | none | C++ | No phase-offset control. `beat_distance` is the deck's own beat fraction, not its offset from the leader |
| Tempo bend ◀ ▶ | `PushButton` on `[ChannelN],rate_temp_down` / `rate_temp_up` | skin | |

## Overview row

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Overview | `Overview` `[ChannelN]` (`BarLineColor`, `PhraseLineColor`, `ShowCueTimes`, `Label*Color`) | skin | Cue flags are drawn natively |
| "bars - 5" hover readout | DJNote's hover readout (`src/waveform/overviewlayout.cpp:43`) | partial | Turned on in Preferences, not by the skin. Style it with `LabelTextColor` and `LabelBackgroundColor` |
| \|◀ jump to start | `PushButton` on `[ChannelN],start` | skin | |
| ◁ ▷ | `PushButton` on `[ChannelN],beatjump_backward` / `beatjump_forward` | skin | |
| Tick icon | `[ChannelN],keylock`, or `[EffectRack1_EffectUnitN],enabled` with the Metronome effect loaded | partial | Mixxx's metronome is an effect that follows the leader tempo, not a per-deck tick. Ask the user which one is meant |

## Transport row

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Play | `[ChannelN],play`, lit by `play_indicator` | skin | |
| CUE | `[ChannelN],cue_default`, lit by `cue_indicator` | skin | |
| CUP | `[ChannelN],cue_gotoandplay` | skin | |
| FLX | `[ChannelN],slip_enabled` | skin | |
| REV | `[ChannelN],reverse` (`reverseroll` for flux-reverse) | skin | |
| Size row `1/4 … 32` | 8× `PushButton`: `[ChannelN],beatloop_X_toggle`, highlighted from `beatloop_size` with `<Transform><IsEqual>X</IsEqual>` | partial | Transforms are display-only (`src/util/valuetransformer.cpp:41-58`). No button sets a size without starting a loop, and loop size and `beatjump_size` are separate |
| `<` `>` around the size row | `[ChannelN],loop_halve` / `loop_double` | skin | |
| IN / OUT / loop | `[ChannelN],loop_in` / `loop_out` / `reloop_toggle`, lit by `loop_enabled` | skin | |
| ↑ collapse | `PushButton` on a skin attribute, wired to the row's `visible` | skin | |

## Hot cue strip

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Segmented bar 1-8 | 8× `HotcueButton` (`<Hotcue>1..8`, `[ChannelN]`). QSS can use the `[type="hotcue"/"loop"…]`, `[light]` and `[dark]` properties | partial | The button sets the cue's colour as an inline stylesheet (`src/widget/whotcuebutton.cpp:162-175`), so QSS can't impose colours by type |

## Cue/move/grid row (Traktor's advanced panel)

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| MOVE / CUE / GRID modes | `PushButton`s on a skin attribute, plus a `WidgetStack` of the `< \| >` sets | skin | |
| MOVE `< \| >` | `beatjump_backward` / `beatjump_forward` | skin | |
| GRID `< \| >` | `beats_translate_earlier` / `beats_translate_curpos` / `beats_translate_later` | skin | |
| CUE `< \| >` | `shift_cues_earlier` / `shift_cues_later` | C++ | These move *all* cues, not the focused one |
| Cue type dropdown | none | C++ | `hotcue_X_type` is read-only (`src/engine/controls/cuecontrol.cpp:2509`) |
| Cue name "AutoGrid" | none | C++ | Labels are strings, and controls only carry doubles. Only the right-click cue menu edits them |
| "1 0:0.0" | `Number` on `[ChannelN],hotcue_focus` for the number | partial | The position is in samples. Showing it as m:ss.s needs C++ |
| MAP | none | C++ | No map/store-to-slot mode |
| Delete | none | C++ | No `hotcue_focus_clear`. Only per-slot `hotcue_X_clear` exists |

## Mixer

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| GAIN | `Knob` or `KnobComposed` on `[ChannelN],pregain` | skin | |
| GAIN dot | `StatusLight` on `[ChannelN],replaygain` with `<Transform><IsEqual>1</IsEqual><Not/>` | partial | Approximates Traktor's autogain indicator |
| HI / MID / LO | `[EqualizerRack1_[ChannelN]_Effect1],parameter3` / `parameter2` / `parameter1` | skin | Kill dots: `button_parameterN` |
| FLTR | `[QuickEffectRack1_[ChannelN]],super1` | skin | On-dot: `[QuickEffectRack1_[ChannelN]],enabled` |
| FLTR dropdown | `EffectChainPresetSelector` on `[QuickEffectRack1_[ChannelN]]` | skin | |
| FX 1 / 2 assign | `PushButton` on `[EffectRack1_EffectUnit1],group_[ChannelN]_enable` (and Unit2) | skin | |
| KEY | `[ChannelN],pitch_adjust` | skin | Dot: `[ChannelN],keylock` |
| Channel fader | `SliderComposed` on `[ChannelN],volume` | skin | |
| VU meter | `VuMeter` on `[ChannelN],vu_meter` plus `peak_indicator` | skin | |
| Crossfader | `SliderComposed` on `[Master],crossfader` | skin | |
| A B / C D labels | static `Label`s | skin | For real assignment, use a 3-state button on `[ChannelN],orientation` |
| `<< < > >>` auto-crossfade | none | C++ | Mixxx only has `[AutoDJ],fade_now` |
| AUX | `[Auxiliary1],pregain` | skin | |
| Headphone icon / MIX / VOL | pixmap, `[Master],headMix`, `[Master],headGain` | skin | |

## Library

| Element | Mixxx equivalent | Status | Note |
|---|---|---|---|
| Search box | `SearchBox` | partial | Its dropdown is search history, not a field filter |
| Folder tree | `LibrarySidebar` | partial | Fully stylable with QSS. The root items (Tracks, Crates, Playlists, Computer…) are fixed by C++ |
| Playlist favorites grid | none | C++ | No widget or control loads a specific playlist |
| Table | `Library` | partial | Column order and visibility come from the user's saved header state, not the skin |
| # column | playlist position column | partial | Shown in playlist views only (`COLUMN_PLAYLISTTRACKSTABLE_POSITION`) |
| Lock icon | BPM lock checkbox inside the BPM column (`bpmdelegate`) | partial | A separate column needs C++ |
| Loaded-deck letter | none | C++ | No "loaded in deck" column |
| Played checkmark | Played column | skin | |
| Cover art | `COLUMN_LIBRARYTABLE_COVERART` | partial | Scaled to the column width, drawn from the top left (`src/library/coverartdelegate.cpp:138-155`). Set the column width to about the row height |
| Title, artist, BPM, key, rating, comment, genre | existing columns | skin | |
| LABEL column | none | C++ | No database field or column (`src/library/columncache.h`) |
| Row height | `[Library],RowHeight` preference | partial | See "Facts" below |
| Selected row | QSS `selection-background-color` on `WTrackTableView` | skin | |
| Footer status bar | none | C++ | No status-bar widget and no library totals |

## Facts

- **A skin can't choose the scrolling waveform type.**
  - `parseVisual` (`legacyskinparser.cpp:1036-1039`) uses the factory's global type (`src/waveform/waveformwidgetfactory.cpp:535`), read once from `[Waveform],WaveformType` (`waveformwidgetfactory.cpp:407-412`). Phosphor is `AllShaderPhosphor` = 26 (`src/waveform/widgets/waveformwidgettype.h:34`).
  - A manifest attribute only writes a config key that is empty (`legacyskinparser.cpp:374-377`).
  - The overview type is a live control (`[Waveform],WaveformOverviewType`, `src/widget/woverview.cpp:98-103`). The spec says the skin leaves both alone.
- **A skin can't set the library row height.**
  - It comes from `[Library],RowHeight` (default 20 px; `src/library/library.cpp:57, 247-248`) and is applied with `setDefaultSectionSize` (`src/widget/wlibrarytableview.cpp:220-223`), which overrides QSS.
  - Changing the library font rescales it (`library.cpp:708-710`).
  - `Library`'s only skin options are `ShowButtonText` and `TrackTableBackgroundColorOpacity` (`src/widget/wlibrary.cpp:20-33`).
- **Fonts go in `res/fonts/`.** `FontUtils::initializeFonts` scans only `<resources>/fonts` (`src/util/font.h:12-41`, called from `src/coreservices.cpp:471`). Qt QSS has no `@font-face`. Fonts there today: Open Sans and Ubuntu.
- **Skins don't scale fluidly.**
  - Layouts stretch through `SizePolicy` and min/max sizes; pixmaps and SVGs keep their size.
  - The global scale factor is fixed at startup (`[Config],ScaleFactor` or `QT_SCALE_FACTOR`, `src/main.cpp:116-140`; `SkinContext::getScaleFactor`, `src/skin/legacy/skincontext.h:248`).
  - `@2x` QSS loading is compiled out (`legacyskinparser.cpp:2197-2226`).
  - LateNight's minimum window is 1280×668 (`res/skins/LateNight/skin.xml:354`).
- **Skin state controls** (`[Skin],…`) come from `<manifest><attributes><attribute config_key=… persist="true">`, as LateNight's `[Skin],show_*` toggles do.
