# Research: how the DJNote app decides key matches

Written 2026-10-21 for issue #83 ("highlight key-compatible library tracks against the master deck"). This documents the existing logic in the **DJNote React app** so a design session can port it. It describes behaviour only. It makes no design decisions for Mixxx.

Source: `D:\Coding Projects\DjAssistant_React_v2.2.7\DjAssistant_React` at commit `6a9ecb7` (app version 1.2.1). Paths below are relative to that root unless they start with `src/track`, `src/engine` or `src/library` under the Mixxx tree. All behaviour claims below were checked by running the real `src/lib/keyUtils.ts`, plus a verbatim copy of `normalizeKey`, under Node 24. The probe output is quoted where it matters.

## 1. Summary

- Every key is stored as **one Camelot string** (`"1A"`..`"12B"`) in `Track.camelotKey`. Matching compares these strings.
- **The reference** is the single "current track", the one the user tapped as *Now Playing*. The app has no decks, playback, tempo or pitch, so the stored key is always the key that gets compared.
- **Two tiers plus none.** `strict` (green) covers the same key, ±1 hour with the same letter, and the relative major/minor. `fuzzy` (yellow) covers ±2 hours and ±7/±5 hours (±1 semitone), always with the same letter. Everything else is `none` (default text colour).
- **Library colouring always shows both tiers.** The user's *Key Matching Strategy* setting (strict/fuzzy) only filters the "Key Suggestions" list. It does not affect the colours.
- Diagonal moves such as 8A→9B or 8A→7B are **never** a match. Mixxx's own `KeyUtils::getCompatibleKeys` does count them.

## 2. Files and functions

| File | Lines | What |
|---|---|---|
| `src/lib/keyUtils.ts` | 9–34 | `keyMap`: the 24-row Camelot ↔ Open Key ↔ traditional table |
| `src/lib/keyUtils.ts` | 36–48 | `keyAliases`: enharmonic and long-form names → the traditional spelling used in `keyMap` |
| `src/lib/keyUtils.ts` | 50–58 | `formatKey(camelotKey, system)`: display only (Camelot → chosen notation) |
| `src/lib/keyUtils.ts` | 63–108 | `getCompatibleKeys(key, mode)`: the set of compatible Camelot strings |
| `src/lib/keyUtils.ts` | 113–126 | `getMatchType(trackKey, currentTrackKey)`: the tier (`'strict' \| 'fuzzy' \| 'none'`) |
| `src/components/TrackList.tsx` | 112–116, 136 | Library list: finds the current track, calls `getMatchType` per row |
| `src/components/TrackCard.tsx` | 245–246, 257–258 | Maps tier → key-text colour class (desktop and mobile layouts) |
| `src/components/NextTrackSuggestions.tsx` | 92, 101, 122, 142 | Same tier colouring on suggestion cards |
| `src/lib/suggestions.ts` | 33–47 | `getSuggestions`: "Key Suggestions" = key in `getCompatibleKeys(current, mode)` **and** \|ΔBPM\| ≤ `bpmRange` |
| `src/lib/transitiveMatching.ts` | 68–77 | `getDiscoveredMatches`: optional key/BPM filter on transitive manual matches |
| `src/lib/audioAnalysis.ts` | 434–492 | `normalizeKey(text)`: tag/metadata text → Camelot (import only) |
| `src/lib/audioAnalysis.ts` | 494–509 | `mapToCamelot(key, scale)`: Essentia analysis result → Camelot |
| `src/hooks/useTrackManagement.ts` | 50–53, 55–99 | `currentTrackId` state and `handleTrackPlay` (sets or toggles the reference) |
| `src/pages/Index.tsx` | 97, 287–305, 888–890 | Passes `keyMatchingMode` and `bpmRange` to `getSuggestions`, and `currentTrackId` to `TrackList` |
| `src/lib/settingsSchema.ts` | 10–11, 34–35, 48 | Defaults: `keySystem: 'camelot'`, `keyMatchingMode: 'fuzzy'`, `bpmRange: 5` |
| `src/components/SettingsPanel.tsx` | 441–469 | UI text for the strict/fuzzy setting |
| `src/components/DJSetPanel.tsx` | 66–91, 181 | A **separate**, hand-written `camelotWheel` table used only by the DJ-set generator (see §9) |
| `tailwind.config.ts` | 56–59 | `neon.blue` / `neon.purple` colour values |

## 3. Key representation

**Canonical form:** a Camelot string, `^(1[0-2]|[1-9])[AB]$`, with A = minor and B = major. The stored key is never a number. `Track.camelotKey: string | null` (`src/types/track.ts:8`).

**The conversion table** (`keyUtils.ts:9-34`) is the standard Camelot / Open Key wheel. Open Key = Camelot + 5 (mod 12, range 1..12), with `m` for minor and `d` for major:

| Camelot | Open | Trad | | Camelot | Open | Trad |
|---|---|---|---|---|---|---|
| 1A | 6m | Abm | | 1B | 6d | B |
| 2A | 7m | Ebm | | 2B | 7d | F# |
| 3A | 8m | Bbm | | 3B | 8d | Db |
| 4A | 9m | Fm | | 4B | 9d | Ab |
| 5A | 10m | Cm | | 5B | 10d | Eb |
| 6A | 11m | Gm | | 6B | 11d | Bb |
| 7A | 12m | Dm | | 7B | 12d | F |
| 8A | 1m | Am | | 8B | 1d | C |
| 9A | 2m | Em | | 9B | 2d | G |
| 10A | 3m | Bm | | 10B | 3d | D |
| 11A | 4m | F#m | | 11B | 4d | A |
| 12A | 5m | Dbm | | 12B | 5d | E |

**Display** (`formatKey`) converts Camelot to the user's `keySystem` (`camelot` | `open` | `traditional`). This changes only the text. Matching always runs on the Camelot string.

**How keys get in:**
- From file tags: `normalizeKey` (`audioAnalysis.ts:434-492`). The steps are:
  1. Trim, and strip a Traktor-style leading zero (`08A` → `8A`).
  2. Match a Camelot string, case-insensitive.
  3. Otherwise strip `major`/`maj` and turn `minor`/`min` into `m`. Turn `X M` (capital M, e.g. `F#M`) into major. Upper-case the first letter.
  4. Look the result up in `keyAliases`.
  5. Look it up in `keyMap` against the traditional, Open Key or Camelot field, case-insensitive.
  6. **If nothing matches, return the input upper-cased.**
- From Essentia analysis: `mapToCamelot(key, scale)` (`audioAnalysis.ts:494-509`). It covers sharps and flats for all 12 roots. An unknown root falls back to **`'8B'` (major) or `'8A'` (minor)**.
- From user edits: `TrackCard.tsx:421-430`. The user picks from the 24 `keyMap` rows or "none" (→ `null`), so edited keys are always valid Camelot.
- From cloud sync: `SyncService.ts:121`, `camelotKey: row.key || null`. **The key is not re-normalised.**

## 4. Matching rules and tiers

Let the reference key be `hN` (hour `h` 1..12, letter `N` ∈ {A,B}). Everything wraps on 1..12.

| Tier | Relationship (Camelot) | Equivalent in semitones (same mode) | Example vs 8A (Am) | Key-text colour |
|---|---|---|---|---|
| **strict** | same key `hN` | 0 | 8A | `text-green-400` = **#4ade80** |
| **strict** | ±1 hour, same letter | ±7 (perfect 5th/4th; shortest ∓5) | 7A, 9A | `text-green-400` = **#4ade80** |
| **strict** | relative major/minor: same hour, other letter | (other mode) | 8B | `text-green-400` = **#4ade80** |
| **fuzzy** | +7 h / +5 h, same letter. Code comment: "Pitch Shift (+/- 1 Semitone)" | +1 / −1 | 3A, 1A | `text-yellow-400` = **#facc15** |
| **fuzzy** | ±2 h, same letter. Code comment: "Energy Shift (+/- 2 Hours)" | ±2 | 10A, 6A | `text-yellow-400` = **#facc15** |
| **none** | anything else, including diagonals (h±1 with the other letter) and missing keys | — | 1B–7B, 9B–12B, 2A, 4A, 5A, 11A, 12A | `text-white/80` (desktop) / `text-white/70` (mobile) = white at 80% / 70% alpha |
| *(row is the reference)* | `isPlaying` overrides the tier | — | — | `text-neon-purple` = **#9d00ff** |

The colours come from `TrackCard.tsx:245` and `:257`. `green-400`/`yellow-400` are the stock Tailwind 3.4 palette (no override in `tailwind.config.ts`; values from `node_modules/tailwindcss/lib/public/colors.js` v3.4.19), and `neon-purple` is defined at `tailwind.config.ts:58`. The tier only changes the **key text colour**. Nothing else on the row changes.

**Precedence:** strict is tested first, then fuzzy (`getMatchType`). The two sets don't overlap, so the order only matters for the exact-equality shortcut.

**Full table from the probe**, colouring every key against 8A and 8B:

```
vs 8A: strict = 7A 8A 8B 9A         fuzzy = 1A 3A 6A 10A        all others none
vs 8B: strict = 7B 8A 8B 9B         fuzzy = 1B 3B 6B 10B        all others none
```

So against any reference, **4 keys are green, 4 are yellow and 16 have no colour.**

**The same rule as semitone arithmetic** (a fact derived from the table, useful for §10). Let `d` = the shortest tonic distance from reference to candidate in −6..+6:
- Both keys in the **same mode**:
  - `d ∈ {0, +5, −5}` → strict.
  - `d ∈ {±1, ±2}` → fuzzy.
  - `d ∈ {±3, ±4, 6}` → none.
- **Different modes:** strict only for the relative key (same Camelot hour), otherwise none.

**Where `mode` matters:** `getCompatibleKeys(key, 'strict')` returns the 4 strict keys. With `'fuzzy'` it returns all 8 (strict ∪ fuzzy). `getMatchType` always checks both. The user setting `keyMatchingMode` (default `'fuzzy'`) is passed only to `getSuggestions` and `getDiscoveredMatches`. The settings text "Define how compatible tracks are highlighted" (`SettingsPanel.tsx:444`) therefore does **not** describe what the code does: highlighting ignores the setting.

**Suggestions vs colouring.** "Key Suggestions" (`suggestions.ts:35-47`) uses the same key set plus a BPM window. A track qualifies if `|current.bpm − track.bpm| ≤ bpmRange` (default 5) or if either BPM is `null`. The result is truncated to `suggestionLimit`. **It is not sorted by tier.** The help text claim that matches are "sorted by how well they blend" (`NextTrackSuggestions.tsx:44`) has no matching code. Library colouring has no BPM condition.

## 5. Reference track, tempo, pitch and key lock

- **Reference:** `currentTrackId` in `useTrackManagement.ts:50`, set by `handleTrackPlay` (`:55-99`). Tapping a track makes it current ("Now Playing" toast). Tapping it again clears it ("Track Paused"). There is exactly one reference. With none, every row is `'none'` (`TrackList.tsx:136`).
- **No effective key:** the `Track` type has no pitch, tempo or key-lock fields, and the app contains no audio playback code. The compared key is always the stored `camelotKey` of both tracks. Tempo enters only the suggestions' BPM filter, never the key comparison.
- The "pitch shift" wording in the fuzzy tier is a *suggestion that the DJ could pitch by a semitone*. The app does not shift any key itself.

## 6. Edge cases (verified by the probe)

**Missing key.** `getMatchType` returns `'none'` if either key is falsy (`keyUtils.ts:114`). The row shows `---`.

**Default keys hide missing data.** Several paths write a default key, which then matches like a real one:
- `DataContext.tsx:310` stores `track.camelotKey || '1A'` when adding a track.
- Free users get `'8A'` (`audioAnalysis.ts:351`).
- Failed analysis stores `'8A'` (`useAudioImport.ts:108`).
- Unknown Essentia roots map to `'8A'`/`'8B'`.

**Comparison is exact and case-sensitive.** Matching compares raw strings. `getMatchType('8a','8A')` → `none`.

**Strings that aren't Camelot.**
- `getCompatibleKeys` parses `parseInt(key)` and `key.slice(-1)`. If the parse fails (e.g. `'Am'`), it returns `[key]`, so only an identical string matches. The test at `keyUtils.test.ts:9-11` fixes this behaviour.
- A leading number with another suffix is treated like Camelot. For example `'1m'` gives `1m,2m,12m,1A,…`, so `getMatchType('1A','1m')` → `strict`. This is an artefact. Stored keys are normally Camelot, so it should not occur in practice.

**`normalizeKey` results** (input → stored):

| Handled | Result |
|---|---|
| `8a`, ` 8A `, `08A`, `08b` | `8A`, `8A`, `8A`, `8B` |
| `1m`, `6d`, `12M` (Open Key, any case) | `8A`, `1B`, `7A` |
| `Am`, `am`, `A minor`, `A min`, `Amin` | `8A` |
| `C`, `c`, `CM`, `C Major`, `Amaj` | `8B`, `8B`, `8B`, `8B`, `11B` |
| `G#m`, `D#m`, `A#m`, `Dbm`, `Db minor` | `1A`, `2A`, `3A`, `12A`, `12A` |
| `Cb`, `B#`, `E#`, `Fb`, `Gb`, `C#`, `D#`, `A#` | `1B`, `8B`, `7B`, `12B`, `2B`, `3B`, `5B`, `6B` |

**Strings that fail normalisation.** These fall back to the input upper-cased, which then matches nothing except the identical string:

| Group | Inputs |
|---|---|
| Out of range | `13A`, `0A`, `010A`, `01m` |
| **Common minor enharmonics** | **`C#m`, `c#m`, `C# minor`, `Gbm`, `Gb minor`**, `Cbm`, `E#m`, `Fbm`, `B#m` |
| Junk | `Cmaj7`, `Am/C`, `8A/9A`, `None`, `n/a`, `x` |

Why the enharmonics fail: `keyAliases` holds `"C# minor"` and `"Gb minor"`, but `normalizeKey` rewrites ` minor` to `m` before the alias lookup. Those alias entries can therefore never match, and there are no `C#m`/`Gbm` entries. C#m (12A) and F#m spelled as Gbm (11A) are both common in tags. Mixxx's parser handles all of these (§10).

**No other notations.** There is no handling of Lancelot modes (`8D`, `8L`…), Unicode ♯/♭, or Rapid Evolution `+50` cents suffixes.

## 7. Tests

| File | Covers |
|---|---|
| `src/lib/__tests__/keyUtils.test.ts` | `getCompatibleKeys`: empty, non-numeric, strict set of 4, wrap 12→1 and 1→12, fuzzy ⊃ strict, +7/+5 and ±2. `getMatchType`: none/strict/fuzzy cases (`3A` vs `8A` fuzzy, `4A` vs `8A` none). `formatKey` round-trips all 24 rows. |
| `src/lib/__tests__/suggestions.test.ts` | `getSuggestions`: key filter, BPM window, null BPM, limit, user-assigned vs auto, strict vs fuzzy (`3A` vs `8A`), reverse matches |
| `src/lib/__tests__/audioAnalysis.test.ts` (lines 94–160) | `mapToCamelot` (incl. `8A`/`8B` defaults) and `normalizeKey` (Camelot, lowercase, `08A`, trad, Open Key, `G#m`, `A minor`, `C Major`) |

No test covers the failing enharmonics in §6, the colour mapping in `TrackCard`, or `transitiveMatching.ts`.

## 8. Verbatim excerpts

`src/lib/keyUtils.ts:63-126` (doc comments trimmed):

```ts
export function getCompatibleKeys(key: string, mode: 'strict' | 'fuzzy'): string[] {
    if (!key) return [];

    const hourPart = parseInt(key);
    const letterPart = key.slice(-1); // 'A' or 'B'

    if (isNaN(hourPart)) return [key];

    const keys = new Set<string>();
    keys.add(key);

    // --- Strict Logic (Always included in Fuzzy) ---
    if (mode === 'strict' || mode === 'fuzzy') {
        // Neighbors (+/- 1 hour, same letter)
        keys.add(`${hourPart === 12 ? 1 : hourPart + 1}${letterPart}`);
        keys.add(`${hourPart === 1 ? 12 : hourPart - 1}${letterPart}`);

        // Relative Major/Minor (Same hour, swapped letter)
        keys.add(`${hourPart}${letterPart === 'A' ? 'B' : 'A'}`);
    }

    // --- Additional Fuzzy Logic ---
    if (mode === 'fuzzy') {
        // Pitch Shift (+/- 1 Semitone) -> +/- 7 Hours
        // +1 Semitone (+7 hours)
        let plus7 = (hourPart + 7) % 12;
        if (plus7 === 0) plus7 = 12;
        keys.add(`${plus7}${letterPart}`);

        // -1 Semitone (+5 hours or -7 hours)
        let minus7 = (hourPart + 5) % 12;
        if (minus7 === 0) minus7 = 12;
        keys.add(`${minus7}${letterPart}`);

        // Energy Shift (+/- 2 Hours)
        let plus2 = (hourPart + 2) % 12;
        if (plus2 === 0) plus2 = 12;
        keys.add(`${plus2}${letterPart}`);

        let minus2 = (hourPart - 2);
        if (minus2 <= 0) minus2 += 12;
        keys.add(`${minus2}${letterPart}`);
    }

    return Array.from(keys);
}

export function getMatchType(trackKey: string, currentTrackKey: string | undefined): 'strict' | 'fuzzy' | 'none' {
    if (!currentTrackKey || !trackKey) return 'none';
    if (trackKey === currentTrackKey) return 'strict';

    // Check Strict Matches First (Green)
    const strictKeys = getCompatibleKeys(currentTrackKey, 'strict');
    if (strictKeys.includes(trackKey)) return 'strict';

    // Check Fuzzy Matches (Yellow)
    const fuzzyKeys = getCompatibleKeys(currentTrackKey, 'fuzzy');
    if (fuzzyKeys.includes(trackKey)) return 'fuzzy';

    return 'none';
}
```

The tier → colour mapping, `src/components/TrackCard.tsx:245-246` (desktop; the mobile copy at `:257-258` is the same except `text-white/70`):

```tsx
<span className={`text-sm font-bold font-mono ${isPlaying ? 'text-neon-purple' : matchType === 'strict' ? 'text-green-400' : matchType === 'fuzzy' ? 'text-yellow-400' : 'text-white/80'}`}>
  {track.camelotKey ? formatKey(track.camelotKey, keySystem) : '---'}
```

Where the reference comes from, `src/components/TrackList.tsx:112-116, 136`:

```tsx
const currentTrack = useMemo(
  () => currentTrackId && allTracks ? allTracks.find(t => t.id === currentTrackId) : null,
  [allTracks, currentTrackId]
);
const currentTrackKey = currentTrack?.camelotKey;
// ...
matchType={currentTrack ? getMatchType(track.camelotKey, currentTrackKey) : 'none'}
```

## 9. Other key logic in the app (not used for colouring)

`DJSetPanel.tsx:66-91` has its own hard-coded `camelotWheel` for the DJ-set generator (`:181`). Each entry lists 4 neighbours **without** the key itself. The pattern for most rows is (h−1)N, (h+1)N, hN′, (h−2)B. For example, `8A: ["7A","9A","8B","6B"]`. It is inconsistent:
- `1B` lists `12A` and `11A`.
- `2B` lists `12A`.
- `1A` and `2A` list `11B` and `12B`.

It does not agree with `getCompatibleKeys` and should not be taken as the matching rule.

## 10. Mixxx context (facts only)

**Representation.** `mixxx::track::io::key::ChromaticKey` (`src/proto/keys.proto:13-39`) is an enum:
- `INVALID = 0`
- `C_MAJOR = 1` … `B_MAJOR = 12`: majors by tonic, chromatic from C
- `C_MINOR = 13` … `B_MINOR = 24`: minors, likewise

Enharmonics collapse to one value (e.g. `F_SHARP_MAJOR` "same as G_FLAT_MAJOR"). The library stores both `key` (text) and `key_id` (the enum as an int) (`src/library/dao/trackschema.h:42-43`).

**`KeyUtils`** (`src/track/keyutils.{h,cpp}`):

| Function | Lines | What it does |
|---|---|---|
| `keyIsMajor`, `keyToTonic`, `tonicToKey` | `.h:41-58` | 0-indexed tonic arithmetic |
| `keyToOpenKeyNumber` / `openKeyNumberToKey` | `.cpp:311-352`, `:284-286` | OpenKey wheel position 1..12 (table `s_openKeyToKeys`, `.cpp:104-119`) |
| `openKeyNumberToLancelotNumber` | `.cpp:260-266` | Lancelot = OpenKey − 5 (wrapped). Mixxx's *Lancelot* is the same numbering as the app's Camelot (Lancelot n with A/B = Camelot nA/nB), and the `keyMap` table above agrees with it row by row. |
| `scaleKeySteps(key, n)` | `.cpp:582-610` | Transpose by n semitones, keeping the mode |
| `shortestStepsToKey(a, b)` | `.cpp:646-672` | Tonic distance in −6..+6, regardless of mode |
| `getCompatibleKeys(key)` | `.cpp:730-774` | Returns 6 keys: self, relative, OpenKey ±1 in the same mode, and OpenKey ±1 of the relative (the diagonals). Test: `src/test/keyutilstest.cpp:275-300`, where Am → {Am, C, Dm, Em, F, G}. |
| `guessKeyFromText(text)` | `.cpp:409-519` | Parses OpenKey (`1m`/`1d`), Lancelot with leading zeros and mode letters (`08A`, `8D`…), and traditional notation with any number of `#`/`♯`/`b`/`♭` plus `m`/`min`/`maj`/mode names. It strips `+50`/`-50` cents suffixes. Anything else → `INVALID`. |

**Per-deck key controls** (`src/engine/controls/keycontrol.cpp`):
- `[ChannelN],file_key` holds the track's stored key.
- `[ChannelN],key` is the **effective** key after pitch. `updateKeyCOs` (`:305-318`) computes it as `scaleKeyOctaves(fileKey, pitchOctaves)`. With key lock on, the pitch ratio comes only from `pitch_adjust`. With key lock off, it follows the tempo ratio (`updateRate`, `:151-300`).
- `visual_key_distance` is the remainder after rounding the effective key to the nearest semitone.

So Mixxx keeps both a stored key and a tempo/pitch-adjusted effective key per deck. The DJNote app has no such distinction (§5).

**Existing compatible-key search.** The library search `~key:<k>` (`kFuzzyPrefix = "~"`, `src/library/searchqueryparser.cpp:59, 226-245`) builds a `KeyFilterNode` from `KeyUtils::getCompatibleKeys` (`src/library/searchquery.cpp:767-787`). It matches on `key_id`. Mixxx has no library key-text colouring today; a search for `KeyColor` finds nothing.

**Where the DJNote rules land on this representation:**

| DJNote rule | In `ChromaticKey` terms |
|---|---|
| strict "same" | `candidate == ref` |
| strict ±1 hour | same mode and `shortestStepsToKey(ref, cand) ∈ {+5, −5}` (equivalently `keyToOpenKeyNumber` ±1 with the same `keyIsMajor`) |
| strict relative | `candidate == openKeyNumberToKey(keyToOpenKeyNumber(ref), !keyIsMajor(ref))`. This is the same value as `relativeKey` inside `getCompatibleKeys`. |
| fuzzy ±1 semitone | same mode and `shortestStepsToKey ∈ {+1, −1}` (= `scaleKeySteps(ref, ±1)`) |
| fuzzy ±2 hours | same mode and `shortestStepsToKey ∈ {+2, −2}` (= `scaleKeySteps(ref, ±2)`) |
| none | everything else, and `INVALID` on either side |

**DJNote strict ⊂ Mixxx `getCompatibleKeys`.** Mixxx's set also has the two diagonals, (h±1) with the other letter, which DJNote rates **none**. DJNote fuzzy keys are not in Mixxx's set at all. Enharmonic spelling problems (§6) do not arise once keys are `ChromaticKey` values.
