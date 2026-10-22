# The Traktor NML format, for playlist import and export

Research notes for a later design session on **NML import/export in DJNote**. Written 2026-10-21 against a real NML export from Traktor on the user's machine, Native Instruments' Traktor Pro 4 manual, third-party NML readers and writers on GitHub, and this repo at branch `djnote` (upstream checked at `upstream/main` 300bd8967c). Nothing here is a design. It collects facts and says where each one comes from.

NI does not publish an NML schema. Every claim is tagged with its evidence class:

- **[O] Observed** in the user's real export, `C:\Users\Jakkaru\Desktop\TheLastOneProject djset final\TheLastOneProject Final.nml` (38,094 bytes, written 2025-09-06 14:10 +03:00, 23 tracks). This is primary data written by Traktor. The user's Traktor data folder is `Traktor 4.0.2` (see `docs/research/jkr-beat-grid-handoff.md`), so the file was most likely written by Traktor Pro 4.0.2. The file itself does not name the version.
- **[D] Documented**: NI's own manual or support articles.
- **[RE] Reverse-engineered by third parties**: open-source readers and writers, their test fixtures, or blog posts. NI has not confirmed any of it.
- **[M] Mixxx code** in this repo.
- **[?] Unknown**: no evidence found.

---

## Summary

- **An NML file is plain UTF-8 XML** with root `<NML VERSION="19">` (the user's export, Traktor 4.0.2) or `VERSION="20"` (later Traktor Pro 4 files in a third-party fixture). The children are `HEAD`, `MUSICFOLDERS`, `COLLECTION`, `SETS`, `PLAYLISTS`, `SORTING_ORDER`, `INDEXING`. A playlist export is a small collection file: it holds a `COLLECTION` with only the playlist's tracks (in playlist order), an empty `SETS`, a `PLAYLISTS` tree with `$ROOT` and one `PLAYLIST` node, and an empty `INDEXING`. It has no `MUSICFOLDERS` [O].
- **A playlist entry points at a collection entry by its absolute path.** `PRIMARYKEY KEY` is `VOLUME + DIR + FILE` from that entry's `LOCATION`, with Traktor's `/:` path separator, for example `C:/:Users/:Jakkaru/:Desktop/:…/:02 - Nature of Reality.flac`. All 23 keys match their `LOCATION`s exactly [O].
- **"Export Playlist" with copied files writes absolute paths to the copies.** Every `LOCATION` in the export points at the destination folder on `VOLUME="C:"` with one `VOLUMEID`. The files' original locations are not recorded anywhere [O]. NI documents only Relocate / Consistency Check for paths that no longer resolve [D]. Whether Traktor resolves the paths relative to the NML file when it is imported from another drive is **unknown** [?].
- **Cue positions are milliseconds, keys are numbered 0–23, and grids are cue entries.** `CUE_V2 START`/`LEN` are milliseconds. `TYPE` is 0 Cue, 1 Fade-in, 2 Fade-out, 3 Load, 4 Grid, 5 Loop [RE, consistent with the six types in the manual [D]]. `HOTCUE` is 0–7 for hotcue buttons 1–8, or −1 for none [RE, O]. The beat grid is a `TYPE="4"` cue (named "AutoGrid" by analysis) plus `TEMPO BPM` [O]. `MUSICAL_KEY VALUE` is 0–11 = C…B major and 12–23 = C…B minor in semitone order. All 15 entries that also carry a key text confirm this [O].
- **Mixxx's existing Traktor reader gets the key wrong by one.** `src/library/traktor/traktorfeature.cpp:372-379` casts `VALUE` directly to Mixxx's `ChromaticKey`, but that enum is 1-based (`src/proto/keys.proto:13-39`). Traktor F#m (18) therefore becomes Mixxx F minor, and Traktor C major (0) becomes INVALID [M, O]. That reader also ignores cues, grids, loudness and play counts. It reads only Traktor's own `collection.nml`, read-only [M].

---

## 1. Sources

| Source | What it gives | Class |
|---|---|---|
| The user's export `TheLastOneProject Final.nml` and the 23 audio files Traktor copied next to it (18 FLAC, 3 WAV, 1 AIF, 1 `.wav.mp4`). The audio files were only listed, never opened. | Everything tagged [O]. All 23 `LOCATION@FILE` values equal the 23 audio file names in the folder. | [O] |
| *Traktor Pro 4 Manual* (English, 170724), [PDF](https://native-instruments.com/fileadmin/ni_media/downloads/manuals/traktor/Traktor-Pro-4-Manual-English-170724.pdf). Page numbers are the printed ones. | Export/Import Playlist (p. 99–100), Export the Collection (p. 116), Relocate (p. 115), cue types (p. 64–66), beatgrid and Open Key (p. 41–42), File Management and Analyze Options preferences (p. 195–199). It does **not** mention the NML format beyond "Playlist file (*.nml)". | [D] |
| NI support: [How to transfer Traktor collections, playlists and settings to another computer](https://support.native-instruments.com/support/solutions/articles/69000879391-how-to-transfer-traktor-collections-playlists-and-settings-to-another-computer) | The collection "includes references to the location of all your tracks". If paths differ, run Check Consistency, then Relocate. | [D] |
| Mixxx `src/library/traktor/traktorfeature.cpp` (same in `djnote` and `upstream/main`, apart from a new `mixxx.traktorfeature.dbpath` setting and a macOS sandbox file picker in upstream). | The only NML code in Mixxx: a read-only importer. | [M] |
| [digital-dj-tools/dj-data-converter](https://github.com/digital-dj-tools/dj-data-converter) @ `a0edb2e` (Clojure, Traktor to/from Rekordbox) | Cue type numbers, ms units, `/:` paths, date format, element order, VERSION 19 writer. | [RE] |
| [wolkenarchitekt/traktor-nml-utils](https://github.com/wolkenarchitekt/traktor-nml-utils) @ `a01d137` (Python, models generated from sample files) | Full element and attribute inventory across Traktor 2–4, and real-looking fixtures including `VERSION="20"`. README.md:23 says Traktor 4.x writes `VERSION="20"`. | [RE] |
| [Segolene-Albouy/Traktor-NML-to-Rekordbox-XML](https://github.com/Segolene-Albouy/Traktor-NML-to-Rekordbox-XML) @ `e1fbccb`, [juanmartin/universal-dj-usb](https://github.com/juanmartin/universal-dj-usb) @ `f27598a` | Writers. Useful mainly as warnings: their key tables disagree with each other and with the real file (§3.7). | [RE] |
| [hellricer, "Decoding TRAKTOR4 field" (2021)](https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html) | Traktor's binary file-tag frame. It has the same cue record (name, display order, type, start, len, repeats, hotcue) and the type enum "0 = CUE, 1 = IN, 2 = OUT, 3 = LOAD, 4 = GRID, 5 = LOOP". | [RE] |
| This repo's earlier research: `docs/research/traktor-waveform.md` §4.3 and §8.5, and `docs/research/jkr-beat-grid-handoff.md` | Our own inspection of the user's 530-entry `collection.nml` (AUDIO_ID is always 256 bytes; `CUE_V2 START` is in ms, checked against the waveform). Traktor does not trim MP3/AAC decoder delay the way Mixxx does. | [RE, ours] |

The user's full `Documents\Native Instruments\Traktor 4.0.2\collection.nml` was **not** re-read for this note (the tool permission was refused). §2 compares the export with third-party fixtures of full collections instead.

---

## 2. Overall structure

```
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<NML VERSION="19">
  HEAD          COMPANY="www.native-instruments.com" PROGRAM="Traktor"
  MUSICFOLDERS  (full collection only)
  COLLECTION    ENTRIES="n"  → ENTRY × n
  SETS          ENTRIES="n"  → SET × n (Remix Deck sets)
  PLAYLISTS     → NODE TYPE="FOLDER" NAME="$ROOT" → SUBNODES → NODE …
  SORTING_ORDER (older files; see below)
  INDEXING      → SORTING_INFO PATH=… → CRITERIA ATTRIBUTE= DIRECTION=
</NML>
```

| Element | Playlist export (user's file) [O] | Full `collection.nml` | Notes |
|---|---|---|---|
| `NML@VERSION` | `19` | `19` in Traktor 3.x and in the user's 4.0.2 export; `20` in later Traktor Pro 4 files (`collection_v4_traktor_format.nml` fixture, entries dated up to 2025-11-29) [RE] | dj-data-converter writes `19` and its spec accepts only 19 (`core.clj:207`, `:240`) [RE]. |
| `HEAD` | `COMPANY="www.native-instruments.com" PROGRAM="Traktor"` | `PROGRAM="Traktor Pro 4"` in the VERSION 20 fixture [RE] | |
| `MUSICFOLDERS` | **absent** | Present but empty in every fixture (`collection_indexing.nml`, `history.nml`) [RE]. The manual's Music Folders list (p. 197) is probably stored here; its child format was not seen [?]. | |
| `COLLECTION` | `ENTRIES="23"`: exactly the playlist's 23 tracks, **in playlist order** | Every track in the collection | One `ENTRY` per file. |
| `SETS` | `<SETS ENTRIES="0"></SETS>` | `SET` elements with `SLOT`/`CELL` children: Remix Deck sets (`collection_v4.nml`) [RE] | Not music tracks. |
| `PLAYLISTS` | `$ROOT` folder → one `PLAYLIST` node | The whole tree: nested `FOLDER`s, `PLAYLIST`s, `SMARTLIST`s (with `SEARCH_EXPRESSION QUERY=…`), and the special `_LOOPS`, `_RECORDINGS` and Preparation lists [RE] | §4. |
| `SORTING_ORDER` | absent | Modelled by traktor-nml-utils as `SORTING_ORDER PATH=…` → `SORTING_DATA IDX= ORD=` (`models/collection.py:730-755`, `1050-1075`), and dj-data-converter allows zero or more (`core.clj:254`) [RE]. None of the fixtures contains one, so it is probably from older versions [?]. | Per-list manual sort state. |
| `INDEXING` | `<INDEXING></INDEXING>` | `SORTING_INFO PATH="$COLLECTION"` / `PATH="<playlist name>"` → `CRITERIA ATTRIBUTE="36" DIRECTION="0"` (`collection_v4.nml`) [RE] | The browser's per-list column sort. The `ATTRIBUTE` numbering is unknown [?]. |

Traktor history files (`History/history_*.nml`) use the same format, with an extra `EXTENDEDDATA DECK= DURATION= STARTDATE= STARTTIME= …` inside each playlist `ENTRY` (`history.nml` fixture) [RE].

---

## 3. `COLLECTION/ENTRY`

Children seen in the export, in this order: `LOCATION`, `ALBUM`, `MODIFICATION_INFO`, `INFO`, `TEMPO`, `LOUDNESS`, `MUSICAL_KEY`, then `CUE_V2`×n [O]. Other versions and fixtures add `LOOPINFO` (before the cues) and `STEMS` (after them) [RE]. dj-data-converter's spec enforces this order (`core.clj:36-62`) [RE].

### 3.1 `ENTRY` attributes

| Attribute | Observed | Meaning |
|---|---|---|
| `TITLE`, `ARTIST` | `TITLE` in 23/23 entries. `ARTIST` is missing from the 3 WAV entries, whose `TITLE` is the file name without extension (e.g. `"07 Dr Fractal - Lands Of Spirit"`) [O]. | Track title and artist. Both are optional (dj-data-converter `core.clj:34-35`). |
| `MODIFIED_DATE` | `2025/9/6` on all 23 entries [O] | Date in `yyyy/M/d` form, no zero padding (dj-data-converter `nml.clj:47`) [RE, O]. |
| `MODIFIED_TIME` | `29237` (18 entries), `39391`–`40107` (5) [O] | Seconds since midnight (dj-data-converter `::time/seconds-per-day`, `core.clj:33`, computed as seconds since the start of the UTC day, `core.clj:209-211`) [RE]. The file's mtime is 11:10:06 UTC and the largest value is 40107 = 11:08:27, which suggests **UTC**. That is an inference [O]. |
| `AUDIO_ID` | Base64 of exactly 256 bytes in all 23 entries [O], and in all 530 entries of the user's collection (`traktor-waveform.md` §4.3) | Meaning undocumented. As 4-bit nibbles it looks like a coarse loudness profile over the track [RE, ours]. Segolene's writer calls it a "fingerprint … Traktor uses for validation" and writes a fixed placeholder (`rekord_to_nml.py:33-37`) [RE, unverified]. |
| `LOCK`, `LOCK_MODIFICATION_TIME` | absent [O] | `LOCK="1"` with an ISO time `2022-10-03T15:01:21` [RE fixtures]. This matches the manual's "Analysis Lock button: Locks all values against future changes by the track analysis" (p. 33) [D]. |

### 3.2 `LOCATION`

```
<LOCATION DIR="/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:"
          FILE="02 - Nature of Reality.flac" VOLUME="C:" VOLUMEID="d41d9f67">
```

- **`DIR`** is the folder path *without* the volume. Every path component is preceded by `/:`, and the path ends with `/:`. The root folder is `"/:"` (regex `(?:/:.+)*/:`, dj-data-converter `nml.clj:9-21`, `:27-28`) [RE, O]. Turning `/:` into `/` gives an ordinary path, which is what Mixxx (`traktorfeature.cpp:26-29`) and `tools/beat_bench.py:40-42` do [M]. Nobody documents *why* Traktor uses `/:`. Our guess is that `:` cannot appear in a file name on Windows or classic Mac OS, so it is safe to use as a marker [?].
- **`FILE`** is the bare file name, XML-escaped (`&amp;` for `&`) [O].
- **`VOLUME`** is the Windows drive letter with a colon (`C:`) [O]. On macOS it is the volume name (`"Macintosh HD"`, `collection_indexing.nml`) [RE]. Mixxx builds macOS paths as `/Volumes/<VOLUME><DIR><FILE>`, relying on `/Volumes/Macintosh HD` being a symlink to `/` (`traktorfeature.cpp:333-341`) [M].
- **`VOLUMEID`** is 8 hex digits on Windows (`d41d9f67` here; `68d949fd` for drive `X:` in a fixture) [O, RE]. It is **not** the drive's volume serial number: `vol C:` on this machine reports `B662-E4EA` [O]. On macOS the fixture has the volume name again (`VOLUMEID="Macintosh HD"`) [RE]. How Traktor derives it, and whether it uses it to find a moved drive, are unknown [?]. dj-data-converter requires `VOLUME` and `VOLUMEID` to be both present or both absent (`location.clj:24-29`) [RE].

### 3.3 `ALBUM`, `MODIFICATION_INFO`

- `ALBUM TRACK="2" TITLE="Paradox Manipulation"` is present in 20/23 entries and absent from the WAVs [O]. traktor-nml-utils also models `OF_TRACKS` (`models/collection.py:22`) [RE].
- `MODIFICATION_INFO AUTHOR_TYPE="user"` is on all 23 entries [O]. Fixtures also have `"importer"` and `"application"` [RE]. This appears to record who last changed the entry, but nobody documents it [?].

### 3.4 `INFO`

| Attribute | Observed [O] | Unit / meaning |
|---|---|---|
| `BITRATE` | `936633` (FLAC), `1411200` (16-bit WAV), `2116800` (24-bit AIF), `1675500` (.mp4) | bits per second. Mixxx divides by 1000 to get kbps (`traktorfeature.cpp:348`) [M]. |
| `GENRE`, `LABEL`, `COMMENT`, `PRODUCER` | free text, e.g. `PRODUCER="© 2023 Timecode/Sangoma ℗ 2023 Timecode/Sangoma"` | Read from file tags. Other attributes modelled by traktor-nml-utils: `REMIXER`, `MIX`, `CATALOG_NO`, `KEY_LYRICS`, `RATING` (`models/collection.py:361-517`) [RE]. |
| `KEY` | in 15/23 entries: `F#m`, `F`, `Em`, `Am`, `G#m`, `D`, `D#m`, and one `2d` (Open Key) | Key **text**. Wherever it is present it agrees with `MUSICAL_KEY` (§3.7). It is missing from all 3 WAVs and 5 FLACs even though those have a `MUSICAL_KEY`. That suggests it comes from file tags, not from analysis [?]. |
| `PLAYCOUNT` | 2–29 | Times played. |
| `PLAYTIME`, `PLAYTIME_FLOAT` | `428` / `427.854431` | seconds. `PLAYTIME = ceil(PLAYTIME_FLOAT)` in all 23 entries [O]. |
| `IMPORT_DATE` | `2025/9/6` on **all** 23 entries | Date added. The export date equals the file's mtime. One entry has `LAST_PLAYED="2025/9/3"`, three days *before* its `IMPORT_DATE`, so the export (or the re-import at the copy's path) **resets** the import date to the export day [O, inference]. |
| `LAST_PLAYED` | `2025/9/3`–`2025/9/6` | Date only, no time. |
| `RELEASE_DATE` | `2023/1/1`, `2021/6/28` | Full date. When only the year is known it is written as `yyyy/1/1`. Mixxx stores the string as `year` (`traktorfeature.cpp:351`) [M]. |
| `COVERARTID` | `017/RE0FIXASKAJ5CBMMX25DBKS2OH1D` (17/23) | A key into Traktor's own `CoverArt/NNN/ID` cache inside the Traktor root folder (`traktor-waveform.md` line 118) [RE, ours]. It is **not portable**: the export folder has no cover files [O]. |
| `FLAGS` | `12` (22 entries), `8` (the `.wav.mp4`) | Undocumented bit field. Fixtures show 2, 12, 14, 28 (on locked entries) and 78 [RE]. Third-party writers just write `12` (`rekord_to_nml.py:97`, `universal-dj-usb nml.py:298`) [RE]. Meaning unknown [?]. |
| `RANKING` | absent (no ratings set) | 0–255, 51 per star. Fixtures have 0, 102, 153, **196**, 204, 255 [RE]. Mixxx divides by 51 (`traktorfeature.cpp:353-364`) [M]. |
| `COLOR` | absent | Track colour index `1`–`7` (Red, Orange, Yellow, Green, Blue, Violet, Magenta per Segolene `consts.py:7-15`) [RE]. |
| `FILESIZE` | **absent** | In fixtures (e.g. `17452` for a 17.4 MB MP3, so KiB) [RE]. The 4.0.2 export omits it [O]. |

### 3.5 `TEMPO`

`<TEMPO BPM="151.999603" BPM_QUALITY="100.000000">` [O]. BPM is a float with 6 decimals and is the tempo of the (single) grid. `BPM_QUALITY` is 100 on every entry; its meaning is undocumented [?]. traktor-nml-utils also models `BPM_TRANSIENTCOHERENCE` (`models/collection.py:823`) [RE].

### 3.6 `LOUDNESS`

`<LOUDNESS PEAK_DB="0.309487" PERCEIVED_DB="-0.614532" ANALYZED_DB="-0.614532">` [O]. These are dB values, and `PERCEIVED_DB == ANALYZED_DB` in all 23 entries. They drive Auto-Gain (`traktor-waveform.md` §1). The reference level, and what `ANALYZED_DB` adds over `PERCEIVED_DB`, are undocumented [?]. The TRAKTOR4 file tag has matching `PKDB`/`PCDB` frames [RE, hellricer].

### 3.7 `MUSICAL_KEY VALUE`: numbering

NI documents the Open Key notation (p. 42: numbers 1–12, `d` = major, `m` = minor) but not the numeric field [D]. The numbering below is confirmed by the export, where each of these entries also carries a key text: **D=2, F=5, G=7 ("2d"), D#m=15, Em=16, F#m=18, G#m=20, Am=21** [O]. The fixtures add Cm=12 ("10m"), Fm=17 ("9m") and Gm=19 ("11m") [RE]. Segolene's `TONALITY_MAP` (`consts.py:17-42`) agrees on all 24 values. hellricer's TRAKTOR4 example pairs `TKEY 10m` with `MKEY 12` [RE]. The rule is **`VALUE = pitch class (C=0 … B=11) + 12 if minor`**.

| VALUE | Key | Open Key | Mixxx `ChromaticKey` | | VALUE | Key | Open Key | Mixxx `ChromaticKey` |
|---|---|---|---|---|---|---|---|---|
| 0 | C | 1d | 1 C_MAJOR | | 12 | Cm | 10m | 13 C_MINOR |
| 1 | C#/Db | 8d | 2 | | 13 | C#m | 5m | 14 |
| 2 | D | 3d | 3 | | 14 | Dm | 12m | 15 |
| 3 | D#/Eb | 10d | 4 | | 15 | D#m/Ebm | 7m | 16 |
| 4 | E | 5d | 5 | | 16 | Em | 2m | 17 |
| 5 | F | 12d | 6 | | 17 | Fm | 9m | 18 |
| 6 | F#/Gb | 7d | 7 | | 18 | F#m | 4m | 19 |
| 7 | G | 2d | 8 | | 19 | Gm | 11m | 20 |
| 8 | G#/Ab | 9d | 9 | | 20 | G#m | 6m | 21 |
| 9 | A | 4d | 10 | | 21 | Am | 1m | 22 |
| 10 | A#/Bb | 11d | 11 | | 22 | Bbm | 8m | 23 |
| 11 | B | 6d | 12 B_MAJOR | | 23 | Bm | 3m | 24 B_MINOR |

So **Mixxx `ChromaticKey` = Traktor `VALUE` + 1** (`src/proto/keys.proto:13-39`) [M].

Third-party tables are unreliable here. Segolene's `KEY_TO_CODE` (`consts.py:44-69`) maps 0 to "10d", which is not Open Key for C. universal-dj-usb's `TRAKTOR_KEY_MAP` (`generators/nml.py:20-58`) maps F#m to 14 and is labelled "educated guesses" [RE].

**Upstream Mixxx bug [M, O]:** `traktorfeature.cpp:372-379` says "Traktor happens to use the same key numbering" and calls `KeyUtils::keyFromNumericValue(VALUE)` (`keyutils.cpp:528-534`). Every key therefore lands one semitone low (F#m becomes F minor), and C major becomes INVALID. The `MUSICAL_KEY` result overwrites the `INFO KEY` text read just before it (`:347`), so the wrong value wins even when correct text is present.

The `INFO KEY` text uses whatever notation the source used. Traktor's "Displayed in Traktor" setting offers Musical, Musical (all sharps) and Open Key; "Written to File Tags" also offers Key Text (p. 199) [D]. The export mixes `F#m` and `2d` [O]. Mixxx's `KeyUtils::guessKeyFromText` parses Open Key (`s_openKeyRegex`, `keyutils.cpp:21-22`, `:409-444`) and traditional notation [M].

### 3.8 `CUE_V2`

```
<CUE_V2 NAME="AutoGrid" DISPL_ORDER="0" TYPE="4" START="137.113678" LEN="0.000000" REPEATS="-1" HOTCUE="0">
```

| Attribute | Meaning | Evidence |
|---|---|---|
| `NAME` | Label. Analysis names the grid marker `AutoGrid`. Unnamed user cues are `n.n.` (all 36 user cues here) [O]. Other fixtures have `Unnamed` and `Beat Marker` [RE]. | [O], [RE] |
| `TYPE` | `0` Cue, `1` Fade-In, `2` Fade-Out, `3` Load, `4` Grid, `5` Loop | dj-data-converter `cue.clj:17-22`; hellricer ("0 = CUE, 1 = IN, 2 = OUT, 3 = LOAD, 4 = GRID, 5 = LOOP") [RE]. The manual lists the same six types (Cue, Fade-In, Fade-Out, Load, Loop, plus Grid Markers) but without numbers (p. 41, 64–66) [D]. The export has only types 0 and 4 [O]. |
| `START` | Position in **milliseconds** from the start of the decoded audio, 6 decimals | dj-data-converter `cue.clj:44`, `:66-72` [RE]. Checked against the waveform in `traktor-waveform.md` §8.5 [RE, ours]. Loop example: `LEN="3809.518965"` at 126.000160 BPM is 8 beats (8 × 60/126 s = 3809.5 ms) in `history.nml` [RE]. |
| `LEN` | Length in ms. Non-zero only for loops (`TYPE="5"`) [RE]. `0.000000` on all cues here [O]. | |
| `HOTCUE` | `0`–`7` = hotcue buttons 1–8 (the manual has "Hotcue 1 – 8", p. 30) [D]. `-1` = a stored cue that is not on a hotcue button (dj-data-converter `cue.clj:29-31`; VERSION 20 grid markers) [RE]. In the export: grid on `0`, user cues on `1, 3, 4, 5, 6, 7` [O]. | [D], [O], [RE] |
| `DISPL_ORDER` | Always `0` here and in the fixtures. Meaning unknown [?]. | |
| `REPEATS` | Always `-1`; hellricer reads it as "-1 = No repeat" [RE]. | |
| `COLOR` | absent [O]. `COLOR="#FFFFFF"` on hotcues in the VERSION 20 fixture [RE]. | |
| child `GRID BPM=` | absent [O]. In VERSION 20, every `TYPE="4"` marker has `<GRID BPM="…">`, which gives each grid marker its own tempo [RE]. | |

**Beat grid.** The manual says: "Beatgrids are defined by one or more Grid Markers, each one setting the tempo until the following Grid Marker"; analysis "will automatically set a single Grid Marker with a constant tempo" (p. 41) [D]. In the export every entry has exactly one `TYPE="4"` cue named `AutoGrid`, its tempo is `TEMPO@BPM`, and there is no `GRID` child [O]. Its `START` ranges from 0.57 ms to 1531.7 ms [O]. `tools/beat_bench.py:53-61` reads exactly this (one `TYPE="4"` anchor, START/1000 s, `TEMPO@BPM`) and skips multi-marker grids [M].

**Grid marker vs hotcue 1 (versioning).** In this VERSION 19 export the grid marker itself sits on `HOTCUE="0"`, so it occupies hotcue button 1 [O]. In the VERSION 20 fixture the grid marker has `HOTCUE="-1"`, and a *separate* `TYPE="0"` cue named `AutoGrid` with `COLOR="#FFFFFF"` sits on `HOTCUE="0"` at the same position [RE]. That matches the TP4 manual's "Grid Marker and Cue Point" Automatic Markers option (p. 199) [D]. Forum posts mention an older "Store Beatmarker as Hotcue" option (e.g. the [Engine DJ bug thread](https://community.enginedj.com/t/bug-engine-dj-4-0-0-puts-all-traktor-hotcues-at-the-beginning-of-the-track/57113)) [RE, anecdotal]. In which Traktor version, and with which preference, each layout is written is unknown [?].

**Order.** Cues are written in ascending `START` order, not by hotcue number. For example, one track (Subtil Alchemy) has a hotcue at 189 ms before its grid at 1373 ms [O].

**Decoder offset.** On MP3 and AAC files, Traktor positions differ from Mixxx's by the decoder delay. Traktor doesn't trim it (~−26 ms on MP3, +47.9 ms of AAC priming), while Mixxx does (`docs/research/jkr-beat-grid-handoff.md:43`) [RE, ours]. Mixxx already corrects Serato cue imports for MP3 decoder cases (`src/track/serato/tags.cpp:74-110`) [M]. All files in this export are lossless except the `.mp4` [O].

### 3.9 Other `ENTRY` children (not in this export)

- `LOOPINFO ORIGINAL_TITLE= ORIGINAL_LOOP_SIZE= ORIGINAL_LOOP_START= SAMPLE_TYPE_INFO=`: loops recorded inside Traktor [RE].
- `STEMS STEMS="<json-ish blob>"`: per-stem settings for `.stem.m4a` files and generated stems [RE].
- `PRIMARYKEY` directly inside a collection `ENTRY` is modelled by traktor-nml-utils (`models/collection.py:943`), but its use there is not shown in any fixture [?].

---

## 4. Playlists

```
<PLAYLISTS><NODE TYPE="FOLDER" NAME="$ROOT"><SUBNODES COUNT="1">
  <NODE TYPE="PLAYLIST" NAME="4skotina extended psyland (2)">
    <PLAYLIST ENTRIES="23" TYPE="LIST" UUID="4afb6b6b7b3f49f8b623932cf3c13770">
      <ENTRY><PRIMARYKEY TYPE="TRACK" KEY="C:/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:02 - Nature of Reality.flac"></PRIMARYKEY>
      </ENTRY>
      …
```

- **Tree.** The root is always `NODE TYPE="FOLDER" NAME="$ROOT"`. A `FOLDER` holds `SUBNODES COUNT=n` → `NODE`s. A `PLAYLIST` node holds one `PLAYLIST ENTRIES=n TYPE="LIST" UUID=<32 hex>`. A `SMARTLIST` node holds `SMARTLIST UUID=` → `SEARCH_EXPRESSION VERSION= QUERY=` [O, RE]. Mixxx flattens the tree into names like `-->Folder-->Playlist` (`traktorfeature.cpp:417-506`) [M].
- **Entries.** Each `ENTRY` holds one `PRIMARYKEY TYPE="TRACK"`. `TYPE="STEM"` appears for stem files in the VERSION 20 fixture [RE]. Order is playlist order [O]. Playlists can repeat a track [?].
- **KEY format.** `KEY = VOLUME + DIR + FILE`, with no separator between `VOLUME` and `DIR` because `DIR` already starts with `/:`. That gives `C:/:Users/:…/:file.flac` on Windows, and presumably `Macintosh HD/:Users/:…` on macOS (Segolene `nml_to_rekord.py:67-78`) [O, RE]. `VOLUMEID` is not part of the key [O].
- **Joining to the collection.** The match is a string compare with the `LOCATION` of a `COLLECTION/ENTRY`. All 23 keys match exactly [O]. Mixxx converts both sides to a filesystem path and looks the track up by path (`traktorfeature.cpp:547-569`) [M]. The export's `COLLECTION` is written in the same order as the playlist [O].
- **Name.** `NODE@NAME` keeps the playlist name (`4skotina extended psyland (2)`). The `.nml` file name (`TheLastOneProject Final`) and the folder come from what the user typed in the export dialog (manual p. 99: "Enter a name for the Playlist to be exported … Enter a destination path") [O, D]. Whether Traktor created the folder itself is unclear [?].

---

## 5. Export with copied files

**What NI documents [D]:**
- Export Playlist (p. 99): "You can export your Playlists including the tracks to import and playing them on other computers with Traktor. Exporting a Playlist creates a folder containing the tracks in the Playlist as well as the Playlist file."
- Import Playlist (p. 99–100), from the context menu or by dragging a `*.nml` onto the Playlists folder: "The Playlist and all contained tracks will be added to your Track Collection."
- Export the Collection (p. 116) has an Export Format drop-down and a "Copy Tracks To Destination" checkbox. Export Smartlist offers "M3U or NML format" (p. 97).
- "File structure mode" (p. 196) controls the copied files' names: None (unchanged), Flat ("01 Artist - Title"), Artist sub-folders, Label sub-folders. The export keeps the original names in one flat folder, which is consistent with **None** [O].
- Paths that don't resolve show up in Check Consistency, and **Relocate** searches a chosen folder or drive for them (p. 115; NI support article).

**What the real file shows [O]:**
- Every `LOCATION` points at the **copy** in the export folder: `VOLUME="C:"`, `DIR="/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:"`, the same `VOLUMEID="d41d9f67"` on all 23. The paths are **absolute**, not relative to the `.nml`.
- The tracks' original locations appear nowhere in the file.
- Analysis data travels with the tracks: grid, BPM, key, loudness, hotcues, play count, last played. `IMPORT_DATE` and `MODIFIED_DATE` are reset to the export day (§3.4). `COVERARTID` still points at the exporting machine's cover cache.
- The audio files are byte copies as far as names go: all 23 `FILE` values equal the names on disk. Whether Traktor writes tags into the copies was not checked, because the audio files were not opened.

**Consequences for another machine or drive (partly [?]):**
- On the same Windows machine, with the folder left in place, every path resolves.
- If the folder moves (another user name, a USB stick on another drive letter, or macOS, where `VOLUME` must be a volume name), the absolute paths no longer resolve. NI's documented fix is Relocate [D]. Forum users report that exporting straight to a USB stick and importing from the stick on another laptop works (search results for [NI community #9522](https://community.native-instruments.com/discussion/9522/how-to-export-playlist-with-all-traktor-information-to-play-on-another-laptop-traktor); the page itself could not be read). That would also work if the drive letter happened to match. It is **not evidence** that Traktor resolves paths relative to the NML.
- An importer that wants to be robust could fall back to `<directory of the .nml>/<FILE>` when `VOLUME+DIR+FILE` does not exist. Nothing in the format forbids or documents that. This is a note, not a design.
- Forum reports, not checked: copying was broken in Traktor 3.6.0 (NML and M3U written, no audio) ([NI community #5764](https://community.native-instruments.com/discussion/5764/traktor-pro-3-6-writing-and-importing-playlists-to-from-usb-stick-not-working-anymore)). Importing a playlist NML into an existing collection does not reliably restore comments in TP4 ([#50457](https://community.native-instruments.com/discussion/50457/traktor-pro-4-playlist-nml-import-export-does-not-reliably-preserve-track-comments)). Only tracks that are in the Collection are copied ([DJ TechTools 2014](https://djtechtools.com/2014/02/24/after-the-show-sharing-sets-in-traktor-serato-dj)).

For comparison, Mixxx's own export writes M3U/M3U8/PLS/CSV/TXT playlists (`src/library/trackset/baseplaylistfeature.cpp:541-563`). It has an option for relative paths (`kUseRelativePathOnExportConfigKey`, `:596`) and a separate "Export Track Files" copy (`:623`, `src/library/export/trackexportworker.cpp:142`) [M].

---

## 6. Encoding, escaping and versioning

**Byte level [O]:**
- First line: `<?xml version="1.0" encoding="UTF-8" standalone="no" ?>`, with a space before `?>`. No BOM, LF line endings, and a trailing newline.
- Non-ASCII is written as raw UTF-8 (`á`, `©`, `℗`; 21 bytes in total). The only entity in the file is `&amp;`. Fixtures also show `&quot;` inside a smartlist query, and an unescaped `>` inside an attribute (`ARTIST="T>I, …"`), which is legal XML [RE].
- Elements are never self-closed. Empty elements are written as `<X ...></X>`. Inside an `ENTRY`, each child is followed by a newline, but the `ENTRY` start tag and `LOCATION` share a line. There is no indentation.
- Numbers: floats have 6 decimals (`"0.000000"`). Dates are `yyyy/M/d` without zero padding.
- Third-party writers copy this layout (no self-closing tags, the same XML declaration; universal-dj-usb `generators/nml.py:186-205`) [RE]. Whether Traktor *requires* it is untested [?]. traktor-nml-utils re-serialises files with `<?xml version='1.0' encoding='UTF-8'?>` and self-closing tags (its `collection_v4.nml` fixture), and claims Traktor reads them back, while warning that "writing NML files hasn't been tested thoroughly enough yet" (README.md:16-17) [RE].

**Versions:**

| | NML 19 | NML 20 |
|---|---|---|
| Written by | Traktor 2.x/3.x [RE], and the user's Traktor 4.0.2 export [O] | Later Traktor Pro 4 (traktor-nml-utils README.md:23; fixture dated up to 2025-11) [RE]. The first version to write it is unknown [?]. |
| `HEAD@PROGRAM` | `Traktor` | `Traktor Pro 4` |
| Grid marker | `TYPE="4"` on `HOTCUE="0"`, tempo only in `TEMPO@BPM` | `TYPE="4"` on `HOTCUE="-1"` with child `<GRID BPM=…>`, plus a separate `TYPE="0"` "AutoGrid" hotcue with `COLOR` |
| Cue colour | none | `COLOR="#RRGGBB"` on cues |
| `INFO@FILESIZE` | absent in the 4.0.2 export; present in 3.x-era fixtures | present |
| Primary key types | `TRACK` | `TRACK`, `STEM` |

Older formats: Traktor 1.x used different files. NI forum threads about importing "old .nml files done with Traktor 1.x" exist, but their content could not be read [?]. Users also report that "the structure of the .nml files changed several times with updates" (search snippet, [NI community](https://community.native-instruments.com/discussion/comment/68081)) [RE, anecdotal].

---

## 7. Annotated excerpt of the real export

Three of the 23 entries (two only partly), and the start of the playlist. `AUDIO_ID` is shortened; comments are ours.

```xml
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<NML VERSION="19"><HEAD COMPANY="www.native-instruments.com" PROGRAM="Traktor"></HEAD>
<!-- no MUSICFOLDERS in a playlist export -->
<COLLECTION ENTRIES="23"><ENTRY MODIFIED_DATE="2025/9/6" MODIFIED_TIME="39391" AUDIO_ID="AawAAAASISIjIREj…jIjMiEAAA==" TITLE="Nature of Reality" ARTIST="Purist &amp; Yabba Dabba"><LOCATION DIR="/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:" FILE="02 - Nature of Reality.flac" VOLUME="C:" VOLUMEID="d41d9f67"></LOCATION>
<!--  MODIFIED_TIME 39391 s = 10:56:31 (UTC, inferred). AUDIO_ID decodes to 256 bytes.
      LOCATION = the copy in the export folder, an absolute path on C: -->
<ALBUM TRACK="2" TITLE="Paradox Manipulation"></ALBUM>
<MODIFICATION_INFO AUTHOR_TYPE="user"></MODIFICATION_INFO>
<INFO BITRATE="936633" GENRE="Trance Trance Trance" PRODUCER="© 2023 Timecode/Sangoma ℗ 2023 Timecode/Sangoma" COVERARTID="017/RE0FIXASKAJ5CBMMX25DBKS2OH1D" KEY="F#m" PLAYCOUNT="3" PLAYTIME="428" PLAYTIME_FLOAT="427.854431" IMPORT_DATE="2025/9/6" LAST_PLAYED="2025/9/6" RELEASE_DATE="2023/1/1" FLAGS="12"></INFO>
<!--  BITRATE in bit/s; PLAYTIME = ceil(PLAYTIME_FLOAT); IMPORT_DATE = the export day; raw UTF-8 © ℗ -->
<TEMPO BPM="151.999603" BPM_QUALITY="100.000000"></TEMPO>
<LOUDNESS PEAK_DB="0.309487" PERCEIVED_DB="-0.614532" ANALYZED_DB="-0.614532"></LOUDNESS>
<MUSICAL_KEY VALUE="18"></MUSICAL_KEY>
<!--  18 = 6 (F#) + 12 (minor) = F#m, matching INFO KEY="F#m". Open Key 4m. Mixxx enum 19. -->
<CUE_V2 NAME="AutoGrid" DISPL_ORDER="0" TYPE="4" START="137.113678" LEN="0.000000" REPEATS="-1" HOTCUE="0"></CUE_V2>
<!--  grid marker at 137.1 ms; tempo from TEMPO@BPM; sits on hotcue button 1 (NML 19) -->
<CUE_V2 NAME="n.n." DISPL_ORDER="0" TYPE="0" START="189611.292427" LEN="0.000000" REPEATS="-1" HOTCUE="7"></CUE_V2>
<!--  plain cue at 3:09.611 on hotcue button 8 -->
</ENTRY>
<ENTRY MODIFIED_DATE="2025/9/6" MODIFIED_TIME="29237" AUDIO_ID="AegAESIiRVVWVERp…pGQyNEQhAAAA==" TITLE="06 Dr Fractal vs The Horrids - Subtil Alchemy"><LOCATION DIR="/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:" FILE="06 Dr Fractal vs The Horrids - Subtil Alchemy.wav" VOLUME="C:" VOLUMEID="d41d9f67"></LOCATION>
<!--  WAV: no ARTIST, no ALBUM, no INFO KEY, TITLE = file name -->
<MODIFICATION_INFO AUTHOR_TYPE="user"></MODIFICATION_INFO>
<INFO BITRATE="1411200" PLAYCOUNT="21" PLAYTIME="488" PLAYTIME_FLOAT="487.750031" IMPORT_DATE="2025/9/6" LAST_PLAYED="2025/9/5" FLAGS="12"></INFO>
<TEMPO BPM="152.000015" BPM_QUALITY="100.000000"></TEMPO>
<LOUDNESS PEAK_DB="0.221213" PERCEIVED_DB="1.032700" ANALYZED_DB="1.032700"></LOUDNESS>
<MUSICAL_KEY VALUE="5"></MUSICAL_KEY>
<CUE_V2 NAME="n.n." DISPL_ORDER="0" TYPE="0" START="189.263953" LEN="0.000000" REPEATS="-1" HOTCUE="1"></CUE_V2>
<CUE_V2 NAME="AutoGrid" DISPL_ORDER="0" TYPE="4" START="1373.474360" LEN="0.000000" REPEATS="-1" HOTCUE="0"></CUE_V2>
<CUE_V2 NAME="n.n." DISPL_ORDER="0" TYPE="0" START="296636.602614" LEN="0.000000" REPEATS="-1" HOTCUE="5"></CUE_V2>
<CUE_V2 NAME="n.n." DISPL_ORDER="0" TYPE="0" START="347162.913215" LEN="0.000000" REPEATS="-1" HOTCUE="4"></CUE_V2>
<CUE_V2 NAME="n.n." DISPL_ORDER="0" TYPE="0" START="372426.068573" LEN="0.000000" REPEATS="-1" HOTCUE="7"></CUE_V2>
<!--  cues sorted by START, not by HOTCUE; a cue may come before the grid marker -->
</ENTRY>
…
<ENTRY MODIFIED_DATE="2025/9/6" MODIFIED_TIME="29237" AUDIO_ID="AiETMjMhERI0Q0RD…9llv//////////////UAAAA==" TITLE="Deep Strangeness 155 Bpm.wav" ARTIST="Opsuruus &amp; The Horrids"><LOCATION DIR="/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:" FILE="Transubtil Rec - Opsuruus - Surreal Contrast - 08 Opsuruus &amp; The Horrids - Deep Strangeness 155 Bpm.wav.mp4" VOLUME="C:" VOLUMEID="d41d9f67"></LOCATION>
…
<INFO BITRATE="1675500" LABEL="Transubtil Rec" COMMENT="Visit https://transubtilrec.bandcamp.com" COVERARTID="006/GQI53LCO000T4ANSJBTSBRDFDQHA" KEY="2d" PLAYCOUNT="18" PLAYTIME="546" PLAYTIME_FLOAT="545.032288" IMPORT_DATE="2025/9/6" LAST_PLAYED="2025/9/5" RELEASE_DATE="2024/1/1" FLAGS="8"></INFO>
<!--  the only entry with FLAGS="8" and the only Open Key text ("2d" = G major) -->
…
<MUSICAL_KEY VALUE="7"></MUSICAL_KEY>
<!--  7 = G major = Open Key 2d -->
…
</ENTRY>
</COLLECTION>
<SETS ENTRIES="0"></SETS>
<PLAYLISTS><NODE TYPE="FOLDER" NAME="$ROOT"><SUBNODES COUNT="1"><NODE TYPE="PLAYLIST" NAME="4skotina extended psyland (2)"><PLAYLIST ENTRIES="23" TYPE="LIST" UUID="4afb6b6b7b3f49f8b623932cf3c13770"><ENTRY><PRIMARYKEY TYPE="TRACK" KEY="C:/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:02 - Nature of Reality.flac"></PRIMARYKEY>
</ENTRY>
<!--  KEY = VOLUME + DIR + FILE of a COLLECTION entry; 23 entries, same order as COLLECTION -->
<ENTRY><PRIMARYKEY TYPE="TRACK" KEY="C:/:Users/:Jakkaru/:Desktop/:TheLastOneProject djset final/:01. Noctune - Progenitors.flac"></PRIMARYKEY>
</ENTRY>
…
</PLAYLIST>
</NODE>
</SUBNODES>
</NODE>
</PLAYLISTS>
<INDEXING></INDEXING>
</NML>
```

Whole-file counts [O]: 23 entries. 59 cues: 23 grid markers, all `AutoGrid` on `HOTCUE 0`, and 36 plain cues, all `n.n.`, on hotcues 1 (×1), 3 (×1), 4 (×9), 5 (×4), 6 (×3) and 7 (×18). No loops, fade, load or unassigned cues. BPMs range from 151.9996 to 156.9999. One `VOLUME` and one `VOLUMEID`.

---

## 8. Mapping notes: NML field → closest Mixxx concept

These are facts about both sides. Nothing here decides how to convert.

| NML | Mixxx concept (code) | Notes |
|---|---|---|
| `ENTRY@TITLE`, `@ARTIST` | `TrackInfo` title / artist (`src/track/trackinfo.h:50`, `:17`) | |
| `ALBUM@TITLE`, `@TRACK`, `@OF_TRACKS` | `AlbumInfo` title (`albuminfo.h:25`); `TrackInfo` trackNumber / trackTotal (`trackinfo.h:51-52`) | |
| `INFO@GENRE`, `@COMMENT`, `@REMIXER` | `TrackInfo` genre, comment, remixer (`trackinfo.h:28`, `:19`, `:43`) | |
| `INFO@LABEL` | `AlbumInfo` recordLabel (`albuminfo.h:22`) | |
| `INFO@PRODUCER`, `@MIX`, `@CATALOG_NO` | no direct field. The nearest are `TrackInfo` composer / subtitle / grouping (`trackinfo.h`) | |
| `INFO@RELEASE_DATE` (`yyyy/M/d`) | `TrackInfo` year, a string, "= release date" (`trackinfo.h:56`) | Mixxx's reader stores the raw string (`traktorfeature.cpp:351`). |
| `INFO@BITRATE` (bit/s) | track bitrate in kbps (Mixxx divides by 1000, `traktorfeature.cpp:348`) | Mixxx normally reads this from the file. |
| `INFO@PLAYTIME(_FLOAT)` (s) | track duration | Mixxx normally reads this from the file. |
| `INFO@RANKING` (0–255) | `TrackRecord` rating 0–5 (`trackrecord.h:58`, `:77-79`) | 51 per star; `196` occurs in the wild [RE]. |
| `INFO@COLOR` (1–7) | `TrackRecord` color, an RGB value (`trackrecord.h:56`) | Mixxx ships a "Traktor Pro Track Colors" palette in the same order: Red `#FA4B35`, Orange `#FF8402`, Yellow `#FFF700`, Green `#00F329`, Blue `#0187FF`, Violet `#A669FF`, Magenta `#FE55EA` (`src/util/color/predefinedcolorpalettes.cpp:65-71`, `:334-345`). |
| `INFO@PLAYCOUNT`, `@LAST_PLAYED` (date) | `PlayCounter` timesPlayed / lastPlayedAt, a QDateTime (`src/track/playcounter.h:13-38`) | NML has no time of day. |
| `INFO@IMPORT_DATE` | `TrackRecord` dateAdded, a QDateTime (`trackrecord.h:52`) | Reset by playlist export (§3.4). |
| `INFO@KEY` (text) | `TrackInfo` keyText (`trackinfo.h:33`); `KeyUtils::guessKeyFromText` (`keyutils.cpp:409`) | Mixed notations. |
| `MUSICAL_KEY@VALUE` (0–23) | `ChromaticKey` 1–24 (`keys.proto:13-39`): **Mixxx = VALUE + 1** | §3.7; upstream reader bug. |
| `TEMPO@BPM` + `CUE_V2 TYPE=4 START` | `Beats::fromConstTempo(sampleRate, FramePos, Bpm, subVersion)` (`src/track/beats.h:248-252`); multi-marker grids → `Beats::fromBeatMarkers` (`:259-264`) | START ms → frames = ms/1000 × sample rate. The Rekordbox importer builds grids from beat positions with its own sub-version (`src/library/rekordbox/rekordboxfeature.cpp:916-920`). |
| `ENTRY@LOCK="1"` | `Track::setBpmLocked` (`src/track/track.h:158`) | Traktor's lock covers all analysed values; Mixxx's lock covers only the BPM. |
| `CUE_V2 TYPE=0` with `HOTCUE` 0–7 | `CueType::HotCue` with hotcue index 0–7 (`src/track/cueinfo.h:11-22`, `kFirstHotCueIndex = 0` at `:33`) | Same 0-based index. Mixxx allows up to 37 hotcues (`NUM_HOT_CUES`, `src/engine/controls/cuecontrol.h:17`). |
| `CUE_V2 TYPE=0` with `HOTCUE=-1` | no "stored but unassigned" cue type. The nearest is `CueType::MainCue` (one per track; `TrackRecord` mainCuePosition, `trackrecord.h:57`) | The Rekordbox importer turns the first memory cue into the MainCue and the rest into hotcues (`rekordboxfeature.cpp:1060-1090`). |
| `CUE_V2 TYPE=5` + `LEN` | `CueType::Loop` with start and end positions (`cueinfo.h:16`; `setHotCue` in `rekordboxfeature.cpp:838-870` picks Loop when an end exists) | |
| `CUE_V2 TYPE=3` (Load) | `SeekOnLoadMode::MainCue` / `FirstHotcue` decide where Mixxx seeks on load (`cuecontrol.h:33-39`) | Mixxx has no per-cue "load" type. |
| `CUE_V2 TYPE=1/2` (Fade-In/Out, for Traktor's auto-mix) | `CueType::Intro` / `Outro` ranges (`cueinfo.h:18-19`), used by AutoDJ | Semantics differ: Traktor's are single points, Mixxx's are ranges. |
| `CUE_V2@NAME` (`n.n.` = unnamed) | cue label | |
| `CUE_V2@COLOR` (v20) | cue colour, `RgbColor` | |
| Cue positions on MP3/AAC | `CueInfoImporter::guessTimingOffsetMillis` hook (`src/track/cueinfoimporter.cpp:18-26`), implemented for Serato (`src/track/serato/tags.cpp:74`) | §3.8 decoder offset. |
| `LOUDNESS@PEAK_DB`, `@PERCEIVED_DB` | `ReplayGain` ratio + peak (`src/track/replaygain.h:24-36`; `trackinfo.h:45`) | Different reference levels; no known conversion [?]. |
| `INFO@COVERARTID` | `CoverInfo` (`trackrecord.h:50`) | Points at Traktor's private cache; not portable. |
| `AUDIO_ID`, `FLAGS`, `MODIFICATION_INFO`, `BPM_QUALITY`, `VOLUMEID` | none | Traktor-internal. |
| `LOCATION` (VOLUME + DIR + FILE) | track location, an absolute path (`TrackRecord`/`track_locations`) | Mixxx's reader: `traktorfeature.cpp:327-343`. |
| `PLAYLISTS` tree | Mixxx playlists are flat: `Playlists(id, name, position, hidden, …)` + `PlaylistTracks(playlist_id, track_id, position)` (`res/schema.xml:65-77`, plus `locked`, `:252`). There are no playlist folders. | Mixxx's Traktor view flattens folders into `-->` paths (`traktorfeature.cpp:425`). |
| `SMARTLIST` | no equivalent (Mixxx crates are manual) | |
| Whole-file import | `TraktorFeature` reads only the newest `Documents/Native Instruments/Traktor x.y.z/collection.nml` (`traktorfeature.cpp:604-655`) into separate `traktor_library` / `traktor_playlists` tables, read-only. Tracks are added to the Mixxx library only when used. | No NML writer exists anywhere in Mixxx. The nearest export job is Engine Prime (`src/library/export/engineprimeexportjob.cpp`). |

---

## 9. Open questions

1. **Relative resolution on import.** When Traktor imports an exported playlist NML from a different drive letter or machine, does it look next to the `.nml` for a `FILE` it cannot find, or does it only offer Relocate? Testing an export moved to another drive would settle it.
2. **`VOLUMEID`.** It is not the volume serial on Windows. How is it derived, and does Traktor use it to find a drive whose letter changed?
3. **NML version boundary.** Which Traktor release first writes `VERSION="20"`? Does a VERSION 20 Traktor still read VERSION 19 grids (no `GRID` child, grid on `HOTCUE 0`) correctly? Does a VERSION 19 Traktor ignore `GRID`/`COLOR`?
4. **Grid-as-hotcue layout.** Which preference or version makes the grid marker sit on `HOTCUE="0"` (the user's 4.0.2) rather than `-1` plus a separate AutoGrid hotcue (the VERSION 20 fixture)?
5. **`INFO@KEY` vs `MUSICAL_KEY`.** Why is the key text missing from 8 entries that have a `MUSICAL_KEY`? Is it from file tags, from analysis, or does it depend on the "Written to File Tags" setting? Which one wins when they disagree?
6. **`FLAGS`, `DISPL_ORDER`, `REPEATS` ≠ −1, `BPM_QUALITY`, `AUTHOR_TYPE`.** Their meanings are undocumented. FLAGS was 8 vs 12 on the one `.mp4` file.
7. **Strictness of Traktor's parser.** Does Traktor accept self-closing tags, a different attribute order, missing `AUDIO_ID`/`MODIFIED_*`/`VOLUMEID`, or `ENTRIES`/`COUNT` values that are wrong? Third-party writers copy Traktor's exact layout, but nobody reports testing the limits.
8. **`IMPORT_DATE` reset.** Does every playlist export rewrite `IMPORT_DATE` to the export day, or was it reset for another reason (for example, the copies being imported as new files)?
9. **Full collection comparison.** The user's own `collection.nml` was not re-read for this note. Compare the same 23 tracks there (original `LOCATION`s, `IMPORT_DATE`, `MUSICFOLDERS`, `INDEXING`, `SORTING_ORDER`) with the export.
10. **Loudness.** How do `PERCEIVED_DB`/`ANALYZED_DB` relate to a ReplayGain reference level, and what does Traktor's Auto-Gain target?
11. **Tags in the copied files.** Does the export write Traktor tags (the `TRAKTOR4` frame) into the copies, depending on the "Tag writing mode" (p. 196)? The audio files were deliberately not opened.
12. **macOS keys.** The macOS `PRIMARYKEY` form (`Macintosh HD/:Users/:…`) comes only from a third-party writer; the fixtures show anonymised values. Confirm it with a real macOS export.

---

## References

- User's export: `C:\Users\Jakkaru\Desktop\TheLastOneProject djset final\TheLastOneProject Final.nml` (read in full; audio files only listed).
- Native Instruments, *Traktor Pro 4 Manual* (English, 170724): https://native-instruments.com/fileadmin/ni_media/downloads/manuals/traktor/Traktor-Pro-4-Manual-English-170724.pdf
- NI support, transferring collections: https://support.native-instruments.com/support/solutions/articles/69000879391-how-to-transfer-traktor-collections-playlists-and-settings-to-another-computer
- Mixxx (this repo): `src/library/traktor/traktorfeature.cpp`, `src/proto/keys.proto`, `src/track/keyutils.cpp`, `src/track/cueinfo.h`, `src/track/beats.h`, `src/track/trackrecord.h`, `src/track/trackinfo.h`, `src/track/albuminfo.h`, `src/track/playcounter.h`, `src/track/serato/tags.cpp`, `src/library/rekordbox/rekordboxfeature.cpp`, `src/library/trackset/baseplaylistfeature.cpp`, `src/util/color/predefinedcolorpalettes.cpp`, `src/engine/controls/cuecontrol.h`, `res/schema.xml`, `tools/beat_bench.py`.
- dj-data-converter @ a0edb2e: https://github.com/digital-dj-tools/dj-data-converter/tree/a0edb2eb3bf7e63784717bffb4020b8eda7873/src/converter/traktor
- traktor-nml-utils @ a01d137: https://github.com/wolkenarchitekt/traktor-nml-utils/tree/a01d13721b4856196c02bfca883635f949cfd932
- Traktor-NML-to-Rekordbox-XML @ e1fbccb: https://github.com/Segolene-Albouy/Traktor-NML-to-Rekordbox-XML/tree/e1fbccbd95cb7c15095b36334f95837466fba8b5
- universal-dj-usb @ f27598a: https://github.com/juanmartin/universal-dj-usb/blob/f27598ae6818da002eb26180dd1fd6a913e64a2b/src/universal_dj_usb/generators/nml.py
- hellricer, "Decoding TRAKTOR4 field": https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html
- DJ TechTools, "After the show: sharing sets in Traktor & Serato DJ" (2014): https://djtechtools.com/2014/02/24/after-the-show-sharing-sets-in-traktor-serato-dj
- Earlier DJNote research: `docs/research/traktor-waveform.md` (§4.3, §8.5), `docs/research/jkr-beat-grid-handoff.md`.

---

## Addendum (2026-10-22): what the installed Traktor Pro 4.0.2 writes today

Checked during the `/grill-with-docs` session for #88 against `Documents\Native Instruments\Traktor 4.0.2\collection.nml` (written 2026-10-21), the History files, and a new playlist export of 14 tracks to `C:\Users\Jakkaru\Documents\test\test.nml` (2026-10-22). The installed program is `Traktor Pro 4.exe`, product version 4.0.2. All of these are [O].

- **VERSION 20 everywhere.** The collection, every History file and the new playlist export are `<NML VERSION="20">` with `PROGRAM="Traktor Pro 4"`. The VERSION 19 export of 2025-09-06 described above therefore came from an older Traktor. The summary's "VERSION 19 = Traktor 4.0.2" is wrong.
- **Flexible beatgrids are real and exported unchanged.** `Salona - Connected` has six `TYPE="4"` markers, each on `HOTCUE="-1"` with its own `<GRID BPM>` (137.004, 119.988, 137.003, 119.997, 137.001, 136.971). `Salona - Leaps of Faith` has five (130.06, 174.00, 130.13, 130.54, 130.94). `TEMPO@BPM` equals the first marker's BPM. This answers open questions 3 and 4 for Traktor 4.0.2.
- **Hotcue button 1 is an ordinary cue.** In VERSION 20 the grid marker is never on a button. Button 1 holds a separate `TYPE="0"` cue: sometimes at the grid marker's position (named `AutoGrid`, `COLOR="#FFFFFF"` in the collection, or `n.n.` in the test export), sometimes at another position (`Bumpy`: hotcue at 71.6 ms, grid at 1836.3 ms).
- **Loops sit on hotcue buttons**: `TYPE="5"` with `LEN` in ms and `HOTCUE` 2 or 6.
- **`LOCK="1"` with `LOCK_MODIFICATION_TIME`** appears on locked entries in the export.
- **A playlist can repeat a track.** `PLAYLIST ENTRIES="15"` against `COLLECTION ENTRIES="14"`, because `Salona - Connected` appears twice.
- **`IMPORT_DATE` is again the export day** (`2026/10/22`). This supports open question 8.
- `FLAGS` values here are 12, 24 and 28. No element is self-closed.
- The copied files keep their original names and their read-only attribute (`03. Atropp & Fisheye - Grão de Areia.flac` is read-only in the export folder).
