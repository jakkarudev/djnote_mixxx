# How Traktor forms its waveforms, compared with Mixxx

Research notes for a Traktor-style **scrolling waveform** (and **overview**) renderer in DJNote. Written 2026-10-17 against Traktor Pro 4.0.2 (installed on the user's machine), the public record, and this repo at branch `djnote`.

Traktor is closed source. Every claim below is tagged with its evidence class:

- **[D] Documented**: Native Instruments' own manuals or files that NI ships.
- **[RE] Reverse-engineered**: from file formats or third-party parsers. Most of the stripe-file findings are **our own byte-level inspection** of the user's Traktor data folders. Nobody else has published them, so treat them as one careful but unreviewed analysis.
- **[V] Visual inference**: from screenshots or rendered images. Plausible, not proven.
- **[?] Unknown**: no evidence found.

Glossary terms (`GLOSSARY.md`): **scrolling waveform** is the zoomed deck waveform, **overview** is the whole-track waveform. Traktor calls these the *Waveform* and the *Stripe*.

---

> **Update (test signals, 2026-10-17):** §8 records black-box tests with generated test tones. They settle most of the open questions and **overturn some earlier guesses in this document**. Height is linear, unsmoothed and signed (not a mirrored envelope). The grain is a sample-density ("phosphor") rendering. Colour comes from a smoothed, continuous spectral blend. Where §8 and an earlier section disagree, §8 wins.

## Summary

- **NI documents almost nothing about the algorithm.** The manual says only that the waveform has "color-coded transients; lighter colors represent high frequency content while darker colors represent low frequency content", that the Waveform and Stripe share one Color Mode, and that the four modes are *Ultraviolet, Infrared, X-Ray and Spectrum* [D]. No band count, crossover frequencies, scaling or envelope type is published.
- **The overview (Stripe) is a stored 1024 × 64 pixel, 4-byte-per-pixel image, not an amplitude list** [RE, our inspection]. Each file is 25 header bytes + 262,144 bytes. Each column has one colour (a smooth blend that varies continuously over the track) and an alpha/brightness that is highest at the centre line and fades towards the edges. That fade is a "glow" or density-like profile, not a solid bar. In 79 % of bright pixels, the 4th byte is within −5 %/+15 % of the Euclidean norm of the other three, so the colour is unit-length (L2) normalised.
- **The scrolling waveform does not appear to be cached on disk** [RE, inference]. The only per-track caches are `Stripes/` (1024 columns per track, far too coarse for a deck view) and `Transients/` (onset lists). The binary contains `Base\Utils\Waveform\WaveformDataGenerator.cpp`, and the hardware-screen QML asks for waveforms per audio stream (per stem). Our best inference is that Traktor computes the deck waveform from the decoded audio at load time. This is not proven.
- **Spectrum colour behaves like an additive RGB blend with low ≈ red, mid ≈ green, high ≈ blue** [V]. Kicks look magenta (bass plus click), hats-only intros look cyan or blue, bass/mid sections look orange. That is the same model as Mixxx's RGB renderer. **The big differences are elsewhere:** temporal smoothing, linear-looking dynamics, L2 rather than max colour normalisation, and a per-pixel brightness texture inside each column.
- **Why Mixxx looks different, from the code:**
  - Each band byte is the *peak* of a 4th-order Bessel band over a ~2.3 ms slice (441 slices/s).
  - The high band and the "all" byte are *compressed* with `pow(x, 0.632)`.
  - The colour is normalised by its *max component*, so balanced slices turn white and single-band slices fully saturated.
  - The renderer the user has configured (`WaveformType 23`, "RGB (textured)") *point-samples* one slice per framebuffer column with no max or smoothing. It also draws a bright axis line.
  - The user's config has the "All" visual gain at 1.5×.

  Together these give the comb, confetti and fat-quiet-parts look described in the Visual comparison section.
- **What is achievable without analyzer changes:** blended colour with L2 normalisation, temporal smoothing (attack/release), linearised highs (Mixxx already ships an `unscale()` table), mirrored mono envelopes, slope-joined columns, and a centre-to-edge brightness gradient. **What needs analyzer changes:** a real density/"grain" texture, RMS envelopes, signed min/max, more or different bands, or different crossovers.

---

## 1. Amplitude and shape formation

| Question | Finding | Class |
|---|---|---|
| Peak vs RMS vs other | **Unknown.** NI never says. The stripe shows a brightness *profile* per column: alpha falls from ≈1.0 at the centre to ≈0.36 at the edge, averaged over loud columns in 40 files, and is non-increasing from centre to edge in ~85 % of columns. A solid peak bar would be flat. A gradient like this is what you get when brightness at height *y* encodes *how often* the signal reaches *y* within the column (a coverage/density or survival-function rendering). A purely stylistic fixed gradient is also possible, but the shape of the gradient changes from column to column (steep in quiet intro columns, flat-topped in loud ones), which argues against a fixed ramp. | [RE] data, [V] interpretation |
| Linear / log / power scaling | **Unknown.** Visually, Traktor's quiet intros are much thinner relative to kicks than Mixxx's, so its scaling is *less compressive* than Mixxx's `pow(x, 0.632)` high band (see §6). It could be linear, or RMS-based (which also widens the loud/quiet ratio compared with peak). A controlled tone test would settle this (see Open questions). | [V] |
| Symmetric vs asymmetric | Stripe columns are nearly symmetric about row ~30.5 of 64. The top extent is on average 2.0 ± 1.1 rows larger than the bottom, which looks like a constant centre offset, not independent halves. We found no sign that L/R are drawn separately as in Mixxx (L up, R down). | [RE] |
| Resolution vs zoom | The stripe is fixed at 1024 columns for the whole track, whatever its length (≈0.46 s per column for a 7.9 min track). For the scrolling waveform, at ~55 px/beat (158 BPM, so ≈6.9 ms/px) the screenshot shows independent 1-px columns with per-pixel variation. The internal data rate and its dependence on zoom are **unknown**. | [RE], [V] |
| Smoothing / interpolation | Visually, Traktor kicks form tapered "lens" shapes across neighbouring columns, and colour changes smoothly over time. That suggests temporal smoothing, a longer analysis window than one pixel, or an envelope follower. **Mechanism unknown.** | [V] |
| Normalisation / auto-gain | `collection.nml` stores `<LOUDNESS PEAK_DB=… PERCEIVED_DB=… ANALYZED_DB=…>` per track (seen in the user's collection), and the TRAKTOR4 file tag has matching `PKDB`/`PCDB` frames ([hellricer 2021](https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html)). These drive Auto-Gain. Whether the waveform is drawn pre- or post-gain, or normalised per track, is **unknown**. | [RE] fields, [?] usage |

## 2. Frequency bands and colour

**Documented [D]:**
- Four colour modes: Ultraviolet, Infrared, X-Ray, Spectrum. "Lighter colors represent high frequency content while darker colors represent low frequency content." The Waveform and Stripe share one Color Mode ([NI manual, Basic Usage Tutorials](https://docs.native-instruments.com/online-guides/traktor-pro-manual/en/basic-usage-tutorials); [Preferences](https://docs.native-instruments.com/online-guides/traktor-pro-manual/en/preferences): "Select from four waveforms Ultraviolet, Infrared, X-Ray, and Spectrum").
- There is **no "Traktor/classic" mode** in the Traktor Pro 3/4 documentation. The four names above are the full list, and they also appear as strings in `Traktor Pro 4.exe` [RE].
- Settings key: `Wavedisplay.ColorMode` in `Traktor Settings.tsi`. The user's value is `3` in TP 3.5.0, 3.10.0 and 4.0.2. If the index follows the manual's order (U=0, I=1, X=2, S=3), that is **Spectrum**, which matches the rainbow look in the screenshot. The mapping itself is **unverified**.

**Secondary/anecdotal descriptions of the modes (conflicting):**
- crossfadr (2011): "The rainbow one … is the proper Spectrum view". The others are "X-Ray (grayscale), Infrared (kind of a burnt yellow)" and "Ultraviolet (blue)", "simplified versions of the same thing" ([crossfadr](https://crossfadr.com/2011/06/21/whats-new-in-traktor-pro-2/)).
- Resident Advisor (TP2 review) says the opposite: monochromatic X-Ray, "full color spectrum spanning (Ultraviolet)", with "Spectrum and Infrared" as single-hue shades ([RA](https://ra.co/reviews/8962)). Given the mode names and the screenshot, crossfadr's description is more likely correct.

**Hardware screens (Kontrol S8/S5/D2), from NI-shipped QML [D]:**
- The hardware waveform (`T.Waveform`) and stripe (`Traktor.Stripe`) are native components. QML gives them a `colorMatrix` with **two colours per band** (`low1`, `low2`, `mid1`, `mid2`, `high1`, `high2`, each RGBA with alpha 100–190, plus `background` on the stripe). Locations under `C:\Program Files\Native Instruments\Traktor Pro 4\Resources64\qml\`: `Screens/S8/Views/Waveform/SingleWaveform.qml:25-38` and `Screens/S8/Views/Waveform/Stripe.qml:7-23`.
- The colour table is in `Screens/Defines/Colors.qml:414-497`. The "Default" entry is low `rgba(24,48,80,180)`/`(24,56,96,190)`, mid `(80,160,160,100)`/`(48,112,112,150)`, high `(184,240,232,120)`/`(208,255,248,180)`: dark blue lows, teal mids, near-white highs. There are 16 more entries, one per track or stem colour, each with the same structure (bright low, lighter mid, near-white high).
- So the hardware renderer does colour per band with alpha, and *recolours at draw time*, since the stripe takes a colour matrix. That implies it gets band-separated data rather than a finished RGB image. The meaning of "1" vs "2" (inner/outer, two layers, or a gradient) is **unknown**.
- The community fork [kokernutz/traktor-kontrol-screens](https://github.com/kokernutz/traktor-kontrol-screens) (`Screens/Defines/Colors.qml:423-510`) copies this table and adds a non-NI "Spectrum-like colors" entry (low orange-red, mid green, high blue).
- Loops are tinted by a QML `Colorize` effect (hue 0.33, saturation 0.55), not by the renderer (`Screens/S8/Views/Waveform/WaveformColorize.qml:59-63`).

**Band count and crossovers: unknown [?].** The QML colour matrix suggests **three bands** (low/mid/high), at least on hardware. No crossover frequencies are published.

**Desktop Spectrum colour [RE + V]:**
- Stripe colour is constant within a column: the normalised colour varies by a median of only 0.04–0.06 between rows. It varies **continuously** between columns: 42–54 distinct colours at 0.1 resolution per track, with smooth transitions. That is a **blended colour per column**, not a dominant-band palette.
- The three colour channels form a unit-length vector scaled by alpha (§4). A unit-length vector is the signature of L2 normalisation of a (low, mid, high) energy vector. If the bytes are B,G,R,A (Qt premultiplied ARGB32 on little-endian), then "R = low, G = mid, B = high" gives memory order high, mid, low, magnitude, and the colours we rendered match the screenshot: teal/cyan intros, pink/magenta kick sections, orange bass/mid sections.

## 3. Layering and drawing

- **Hardware:** three bands, each with two RGBA colours with partial alpha. This points to overlapping translucent band layers [D for the inputs; layering itself is inference].
- **Desktop scrolling waveform** (screenshot at native pixels, 6× crop) [V]:
  - 1-px columns, one hue per column.
  - Inside each column the **brightness varies pixel by pixel**: a speckled, grainy fill in orange bass/mid sections, smooth bright gradients in magenta kick columns.
  - No outline and no centre line.
  - Kicks are tapered blobs that span many columns.
- **Desktop stripe** [RE]: one silhouette per column (all four channels have the same non-zero extent) with an alpha gradient from the centre outward. There are no separate nested band shapes in the stored image.

So for desktop Spectrum the evidence points to **one blended-colour shape per column with a brightness texture**, not stacked band silhouettes. The hardware screens may differ.

## 4. Stored data

All from the user's `Documents\Native Instruments\Traktor 4.0.2\` (also present in the 3.5.0 and 3.10.0 folders, in the same format). The format has no public documentation that we could find. A 2010 DJ TechTools thread only notes that stripes and transients live outside the audio files ([DJTT](https://forum.djtechtools.com/showthread.php?t=22289)).

### 4.1 `Stripes/NNN/<28-char ID>` (overview) [RE, ours]

All 248 files (TP 4.0.2) are exactly **262,169 bytes**, and all share the same first 21 header bytes. TP 3.5.0 (738 files) and TP 3.10.0 (917 files) use the same format.

| Offset | Size | Value | Interpretation |
|---|---|---|---|
| 0 | u8 | `0x06` | unknown |
| 1 | 4 | `PRTS` | "STRP" reversed. TRAKTOR4 tag frame IDs are also reversed ([hellricer](https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html)) |
| 5 | u32 LE | 2 | version? |
| 9 | f32 LE | 3.5 | unknown |
| 13 | u32 LE | 1024 | image width (columns), and/or block size |
| 17 | u32 LE | 64 | image height (rows) |
| 21 | u32 LE | N (e.g. 20439) | **number of 1024-sample blocks in the track**. N·1024/44100 matches the time of the last transient in the same-named `Transients` file: median ratio 0.9994, range 0.916–0.9999, over 248 pairs |
| 25 | 262,144 | pixels | 1024 columns × 64 rows × 4 bytes, **column-major** |

Evidence for the layout: zero (background) pixels come in runs that repeat every ~64 entries (1025 runs per file), because the bottom gap of column *i* joins the top gap of column *i+1*.

Pixel bytes `(c0, c1, c2, c3)`:
- `c3` is almost always the largest. `max(c0..c2)/c3 ≤ 0.89` across all files, so the data is valid as premultiplied alpha.
- `c3 ≈ ‖(c0,c1,c2)‖₂`, within −5 %/+15 %, for 79 % of pixels with `c3 ≥ 60` (2.9 M sampled pixels).
- The normalised colour `(c0,c1,c2)/c3` is nearly constant down a column.
- Alpha profile from centre to edge, averaged over 40 files: 0.97, 0.93, 0.89, 0.85, 0.79, 0.74, 0.68, 0.61, 0.51, 0.36.

(Correction, §9: `c3` is about 2.0 x `c1`, not the colour's norm, so the premultiplied reading below is doubtful.)

There are two readings. We cannot yet tell them apart:
1. A premultiplied BGRA image rendered in the colour mode that was active at analysis time.
2. Mode-neutral `(band, band, band, magnitude)` data that the renderer recolours. The hardware `colorMatrix` on `Traktor.Stripe` favours this.

The experiment in Open questions (Q1) would settle it.

The ID in the filename is not in `collection.nml`, and its derivation is **unknown**. `Coverart/` uses the same `NNN/ID` scheme, which `COVERARTID` does reference. (Digital DJ Tips states that Traktor also embeds stripe data in audio-file tags ([article](https://www.digitaldjtips.com/dj-software-secrets/)). We have not verified this.)

Minimal reader, for reproducing the findings:

```python
import struct
raw = open(path, 'rb').read()
assert raw[1:5] == b'PRTS' and len(raw) == 25 + 1024 * 64 * 4
blocks = struct.unpack_from('<I', raw, 21)[0]        # track length / 1024 samples
px = lambda x, y: raw[25 + 4 * (x * 64 + y): 29 + 4 * (x * 64 + y)]  # (c0, c1, c2, c3)
```

### 4.2 `Transients/NNN/<same ID>` [RE, ours]

Each file is `u8 version (=5)`, then `u32 count`, then `count × {f64 time_s, f32, f32}`, so its size is exactly `5 + 16·count`. This holds for all 309 files. Times increase monotonically. Example: 3678 onsets over 474 s, about 7.8 per second. The two floats are probably strength or confidence values, but their meaning is unknown. This is onset data, presumably for beat and grid analysis, **not waveform data**.

### 4.3 `collection.nml` [RE, ours]

- `ENTRY@AUDIO_ID`: always 256 bytes of base64 (530 of 530 entries). As 4-bit nibbles it looks like a coarse loudness profile over the track. The name suggests it identifies tracks (finding moved files). It is not display data.
- `LOUDNESS@PEAK_DB/PERCEIVED_DB/ANALYZED_DB`: loudness values (§1).
- No waveform data in the NML. Tools such as [traktor-nml-utils](https://github.com/wolkenarchitekt/traktor-nml-utils) parse the NML but not stripes.

### 4.4 Is the scrolling waveform stored or computed?

**Probably computed when the track loads** [inference from RE]:
- There is no per-track cache other than Stripes, Transients and Coverart. `%LOCALAPPDATA%\Native Instruments\Traktor` holds only streaming and stems temp folders.
- The binary contains `D:\jws\…\sources\Base\Utils\Waveform\WaveformDataGenerator.cpp`, `onWaveformAnalyseCompleted` and `updateStripeAndTransientsFromTrack`.
- Hardware `T.Waveform` instances take an `audioStreamKey` per stem stream (`StemWaveforms.qml`), so waveforms exist per decoded audio stream.

The manual only says "Once a track has been analyzed, the Waveform and the Stripe will be displayed" [D]. For contrast, Rekordbox *does* store scrolling waveforms (`PWV5` colour, `PWV7` 3-band), documented by reverse engineering in [crate-digger `rekordbox_anlz.ksy`](https://github.com/Deep-Symmetry/crate-digger/blob/main/src/main/kaitai/rekordbox_anlz.ksy) (lines 384–435).

## 5. Existing reimplementations and related work

- **No public Traktor stripe parser or Traktor-waveform clone found** (GitHub repo and code search, web search). The closest public material:
  - NI's own QML for the hardware screens, mirrored in [kokernutz/traktor-kontrol-screens](https://github.com/kokernutz/traktor-kontrol-screens) and [ErikMinekus/traktor-kontrol-screens](https://github.com/ErikMinekus/traktor-kontrol-screens). Rendering is native, so the QML only shows the colour inputs.
  - TRAKTOR4 tag decoding: [hellricer 2021](https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html), Kaitai-based, with no waveform frames identified.
- **Mixxx work on new waveform styles** (all open unless noted):
  - [#13075](https://github.com/mixxxdj/mixxx/pull/13075) "3-band waveform with moving average" (closed). Overlapping 3-band layers with adjustable smoothing.
  - [#14325](https://github.com/mixxxdj/mixxx/pull/14325) "Add RMS signal analysis on Waveform". Adds RMS sampling, which the protobuf already allowed. The author found RMS "provide[s] better visualisation … on electronic music".
  - [#16963](https://github.com/mixxxdj/mixxx/pull/16963) "Perceptual 3-band waveform mode". New analysis profile: per-band RMS, instant-attack/exponential-release envelopes, per-band transfer LUTs, tuned against Rekordbox. Needs re-analysis.
  - [#17141](https://github.com/mixxxdj/mixxx/pull/17141) "RGB 3-band". Renderer-only. Mirrored peak envelopes with attack/release, neighbouring pixels joined with slopes, uses existing data.
  - [#17175](https://github.com/mixxxdj/mixxx/pull/17175) "Use EQ crossover settings for waveforms". Makes the analyzer crossovers configurable.
  - Also [#17129](https://github.com/mixxxdj/mixxx/pull/17129) (stacked EQ scaling), [#15516](https://github.com/mixxxdj/mixxx/pull/15516) (stacked overview) and [#14998](https://github.com/mixxxdj/mixxx/pull/14998) (disable EQ influence).
  - Issue [#11833](https://github.com/mixxxdj/mixxx/issues/11833) "Waveforms like RekordBox". A maintainer (uklotzde) ported filtering and colour mapping from the [audio-viz](https://crates.io/crates/audio-viz) crate with "obvious" improvements.
  - Issue [#6352](https://github.com/mixxxdj/mixxx/issues/6352) explains the high-band `pow` scaling.
- **No Mixxx issue or PR asks specifically for a "Traktor-like" waveform.** Requests and PRs all target Rekordbox. Searched `mixxxdj/mixxx` issues for "traktor waveform".

## 6. How Mixxx forms its waveform (this repo)

### 6.1 Analysis (`src/analyzer/analyzerwaveform.*`)

- **Bands:** 4th-order Bessel low-pass at 600 Hz, band-pass 600–4000 Hz, high-pass at 4000 Hz (`src/analyzer/analyzerwaveform.cpp:13-15`, `:156-158`). Butterworth-8 is commented out. The gentle slopes mean the bands overlap a lot.
- **Slice rate:** 441 visual samples per second (`analyzerwaveform.cpp:59`, `src/waveform/waveform.cpp:47-63`), so each slice is 100 frames at 44.1 kHz (~2.27 ms) and 108.8 frames at 48 kHz.
- **Envelope:** for each slice and each channel (L and R separately), the **max of |x|** for the full signal and for each band ("Take max value, not average", `analyzerwaveform.cpp:196-222`). RMS code exists only as a comment (`:203-212`).
- **Scaling to bytes:** `byte = min(255, 255·scaleSignal(peak) + 0.5)` (`src/analyzer/analyzerwaveform.h:73-84`). `scaleSignal` is linear for low and mid, but **`pow(x, 0.632)` for high and for "all"** (`analyzerwaveform.h:20-28`). That turns −20 dB (0.1) into 0.23 of full height, and −40 dB into 0.054.
- **Stored per slice:** `struct {u8 low, mid, high, all}` × 2 channels, interleaved L/R (`src/waveform/waveform.h:14-28`). There is no RMS, no signed min/max, and no distribution.
- **Overview:** 3840 samples per track (2 × 1920), each the **mean of the slice peaks** in its range (`analyzerwaveform.cpp:61`, `analyzerwaveform.h:96-108`). It can be normalised (`[Waveform] OverviewNormalized`).
- **Gain at draw time:** height × `total_gain` (pregain × ReplayGain, `src/engine/enginepregain.cpp:118-121`, read at `src/waveform/renderers/waveformwidgetrenderer.cpp:96`) × the "All" visual gain from preferences. Some renderers also apply a ×2 "compensation" (`src/waveform/renderers/waveformwidgetrenderer.h:115-129`). Per-band gain follows the EQ knobs and kills (`src/waveform/renderers/waveformrenderersignalbase.cpp:172-228`).

### 6.2 Renderers (`src/waveform/renderers/allshader/`, `res/shaders/`)

All the vertex renderers take, for each screen pixel, the **max over the slices that fall in that pixel** (e.g. `waveformrendererrgb.cpp:117-150`). They then draw one solid 1-px rectangle per channel: L above the axis, R below.

| Type (pref #) | Height | Colour | Notes |
|---|---|---|---|
| RGB (17) | `all` byte (compressed) | `low·Clow + mid·Cmid + high·Chigh`, **normalised by max component** (`waveformrendererrgb.cpp:158-196`) | EQ scales height by the gained/ungained L2 ratio (`:158-178`) |
| L/R RGB (18) | same, L and R both drawn upward from the axis | same | `waveformrendererlrrgb.cpp:163-192` |
| Filtered (19) / Stacked (25, `RGBStackedWaveformWidget`) | separate low, mid, high shapes, drawn in that order on top of each other | flat colour per band (stacked uses the RGB skin colours) | `waveformrendererfiltered.cpp:108-186` |
| HSV (21) | `all` | one skin hue; saturation and value from the high and low shares | `waveformrendererhsv.cpp:138-159` |
| Simple (20) | `all` | flat signal colour | `waveformrenderersimple.cpp:116-139` |
| Textured RGB (23) / Filtered (22) / Stacked (24) | RGB: `‖(low,mid,high)‖/√3` (`res/shaders/rgbsignal.frag:69-77`). Stacked: low, low+mid and low+mid+high stacked outward (`stackedsignal.frag:100-135`) | same max-normalised blend (`rgbsignal.frag:81-88`); a second "unscaled" copy at 40 % alpha behind it (`:90-122`) | **One slice per framebuffer column, `floor()` index, `GL_NEAREST`, no max** (`rgbsignal.frag:34-35`, `waveformrenderertextured.cpp:414-415`). Rendered to a 4× oversized framebuffer (`:192`), then drawn with `GL_LINEAR` (`:475-476`). Axis line drawn ±4 framebuffer px around the centre (`rgbsignal.frag:113-116`) |

Mixxx has an `unscale()` table that undoes the `pow` scaling (`src/waveform/renderers/waveformrenderersignalbase.h:30-37`). All renderers leave it commented out (e.g. `waveformrenderersimple.cpp:124-126`).

**The user's setup:** `%LOCALAPPDATA%\Mixxx\mixxx.cfg` has `WaveformType 23` (Textured RGB) and `VisualGain_0 1.5` (All gain ×1.5). This assumes the fork reads the default settings path.

### 6.3 What Mixxx's data supports vs what Traktor seems to do

| Traktor characteristic | Supported by existing Mixxx data? |
|---|---|
| 3-band blended colour per column | **Yes**: low/mid/high bytes exist. Needs L2 normalisation instead of max, and probably `unscale()` on the high band |
| Smooth colour and amplitude over time (lens-shaped kicks) | **Yes, at render time**: attack/release or moving-average filtering over slices, as #17141 and #13075 do, plus slopes between columns |
| Less compressive dynamics | **Mostly**: undo `pow(0.632)` with `unscale()` and drop the extra ×1.5. Low and mid are already linear peaks. RMS-like contrast needs the analyzer (#14325) |
| Symmetric mono envelope | **Yes**: take the max of L and R (the RGB renderer already does this for colour) |
| No axis line | **Yes**: skin `AxesColor` alpha, or remove it in a new renderer |
| Centre-bright, edge-dim gradient inside the shape | **Yes, approximately**: brightness as a function of \|y\|/height in the renderer |
| Real per-pixel density or "grain" (how often each height is reached within a column) | **Partly**: at ~3 slices per pixel at deck zoom, the in-pixel distribution of slice peaks is too coarse. A faithful version needs a higher slice rate, per-slice signed min/max, or a stored histogram, i.e. **analyzer changes** |
| Different crossovers or band count | **No**: needs re-analysis (#17175 makes crossovers configurable) |
| Stripe-style 1024 × 64 pre-rendered overview | Not needed. Can be drawn from the existing summary |

---

## 7. Visual comparison (user screenshots, 2026-10-17)

Sources:
- `SCREENSHOTS/waveforms/Screenshot 2026-10-17 203136.png`: Traktor Pro 4, CYK "Gigando", deck B at 04:41, 158.36 BPM (+5.6 %), Spectrum.
- `…203129.png`: Mixxx/DJNote, the same track at 4:45.5, 147.95 BPM, LateNight, Textured RGB.

Both are at ~55–58 px per beat, so ≈6.9–7.0 ms per screen pixel. That puts **≈3 Mixxx slices in each pixel** (441/s × 7 ms). The 6× native-pixel crops were viewed directly. Everything in this section is **[V]**, except where a Mixxx code reference explains a feature.

1. **Texture.** Traktor's fill is **grainy**: brightness changes pixel by pixel inside a column, with dark speckles in orange sections and smooth bright gradients in magenta kick columns. Mixxx columns are solid, apart from dark tips at the top and bottom of many columns.
   - Mixxx side: the dark tips are consistent with the textured renderer's 4× framebuffer being minified with `GL_LINEAR`. The 4 sub-columns per pixel have different heights, so the tips blend to partial coverage. The 40 %-alpha "unscaled" layer (`rgbsignal.frag:118-122`) adds to this.
   - Traktor side: the stored stripe has the same kind of per-pixel brightness gradient (§4.1). A density or coverage rendering (brightness = fraction of sub-windows whose envelope reaches that height) would produce exactly this speckle when there are few sub-windows per pixel. So would a deliberate noise or dither texture. **Which one is unknown.** We found no public source describing a "density", "dotted" or "grainy" Traktor waveform style.
2. **Envelope coherence.** Traktor kicks are tapered blobs. Mixxx shows independent spikes (a comb).
   - Mixxx side: each slice is a 2.27 ms peak with no smoothing. The textured renderer **point-samples** one slice per framebuffer column instead of taking a max (`rgbsignal.frag:34-35`), so neighbouring pixels show unrelated slices, and the result flickers while scrolling. The vertex RGB renderer (type 17) takes a max over each pixel and would be noticeably less spiky. Even then, without attack/release smoothing it stays more comb-like than Traktor.
3. **Dynamic contrast.** Traktor's quiet intro is thin and its kicks tall. Mixxx's intro is fat.
   - Mixxx side: the textured height is `‖(low,mid,high)‖/√3`, with the high band compressed by `pow(0.632)` (`analyzerwaveform.h:27`). A hats-heavy intro is mostly high band, so it gets the biggest boost. On top of that: ×2 compensation (`getGains(…, true …)`, `waveformrenderertextured.cpp:305`), `VisualGain_0 = 1.5`, and ReplayGain in `total_gain`.
   - Traktor's scaling: **unknown**, but visibly less compressive.
4. **Colour.** Traktor's colour drifts smoothly: magenta kicks, orange bass/mid, an intro that goes cyan → blue → green → yellow → orange. Mixxx flickers between columns, with thin bright green and white "confetti" lines and a bright centre line.
   - Mixxx side: colour comes from per-slice band peaks (2.27 ms, so the high band jumps around), boosted by `pow`, point-sampled, then **normalised by the max component**. A slice where one band briefly dominates becomes fully saturated green, and a balanced slice becomes white.
   - Traktor side: the stripe's colours are L2-normalised, which keeps balanced mixes muted rather than white. They also change slowly, which is consistent with smoothing.
   - The hue logic itself (kick = magenta = low + high, hats = blue/cyan) is the **same in both programs**. So the colour difference comes from timing and normalisation, not from the palette.
   - The centre line is Mixxx's axis, drawn ±4 framebuffer px (`rgbsignal.frag:113-116`).

Caveats: the positions differ by ~4 s and the tempos differ, so the content under the same pixels is not identical. JPEG/PNG scaling was not involved, because the crops are native pixels.

---

## 8. Test-signal results (2026-10-17)

**Method.** `tools/waveform_test_signals.py` generates 44.1 kHz stereo WAVs with known content: tones, a sweep, level steps, sine vs square, clicks and kicks, sine vs noise, and L/R/inverted stereo. The user loaded each file in Traktor Pro 4.0.2 (Spectrum colour mode), screenshotted the deck, and we read the matching `Stripes/` files written during analysis. Screenshots are in `waveform-tests/<test>/`. Evidence here is **[RE]** for the stripe numbers and **[V]** for the screenshots.

Caveats:
- The zoom was not constant across tests: about 115 px/s for Q2a and 234–243 px/s for Q5 and Q6.
- Only one 33 s stripe exists for the three Q3 files. Its colour matches 8 kHz, so it is Q3c's. Traktor seems to key stripes on something the three files share; all three start with identical silence.
- The gain test (Q8) was not done: the only gain change visible is on the empty deck B.

### 8.1 Shape and height

| Finding | Evidence |
|---|---|
| **Height is linear in peak amplitude.** | Stripe rows for 8 kHz steps at 0/−6/−12/−18/−24 dBFS: 62, 32, 16, 8, 4 (halves per −6 dB). The deck view halves likewise (Q3a/Q3b screenshots). |
| **Peak, not RMS.** | 100 Hz and 1 kHz squares are the same height as sines at equal peak level (stripe: 32 rows each; deck: equal). White noise with peak −10.2 dBFS gives 20 rows, matching linear peak (32 · 10^(−4.2/20) ≈ 19.7). |
| **Signed, not mirrored; positive is drawn downward.** | A single positive-sample click draws on **one side** of the centre line only, the lower one (confirmed in the Q5 deck screenshot and on 4 of 5 real tracks, 2026-10-18), in both the stripe (rows 30–58) and the deck. At ~240 px/s the deck draws the 60 Hz kick's individual cycles, up and down, like an oscilloscope. |
| **No smoothing of height.** | A 50 ms kick occupies ~3 stripe columns (≈105 ms) and ~12 deck px at 234 px/s. Tone blocks have vertical edges. There is no attack/release on the shape. |
| **Mono sum (L+R).** | L-only and R-only at −6 dBFS draw half height (16 rows). Both draw 32. L = −R draws **nothing** (centre line only). |
| **Full scale.** | 0 dBFS fills the stripe's 64 rows and the deck lane. |

### 8.2 The grain: sample-density ("phosphor") rendering

Brightness inside a column is proportional to **how many samples (or sample-to-sample line segments) pass through that pixel**, with each RGB channel clipped at full scale. The evidence:

- **Square waves are hollow boxes.** A 100 Hz square in the deck view is a dark, nearly empty box with bright **yellow** top and bottom edges, and the stripe shows the same outline. A square wave sits at ±peak almost all the time, so all the hits land on the edges.
- **The 1 kHz square is solid with brighter edges.** Its body is blue and its edges are **cyan**. With about 8 transitions per column, the interior gets some hits and the edges get many.
- **Sines have bright edges and a darker middle** at low frequencies (100 Hz, 160 Hz, the 60 Hz kick). A sine spends most of its time near its peaks. At 1 kHz the body is uniformly bright, because enough samples per pixel saturate it.
- **Some sines produce horizontal lines.** 630 Hz (exactly 70 samples per cycle) shows horizontal bands, while 1 kHz (44.1 samples per cycle, so the phase drifts) does not. A periodic sample grid hits only a few heights.
- **White noise is uniformly filled** with ragged edges. Its sample values are spread evenly.
- **Denser means lighter, not just brighter.** Orange saturates to yellow and blue to cyan. That is what you get from adding `colour × hits` and clipping each channel at 1. It explains the yellow/orange speckle in real bass-heavy music.

The low-frequency "grain" in the original screenshots is therefore real signal structure: within a ~4–9 ms column, bass covers only part of a cycle, so the hits are uneven. It is not a decorative texture.

### 8.3 Colour

- **Continuous with frequency.** It is not three flat blocks. Stripe unit colour (R,G,B)/‖·‖ for tones at −6 dBFS:

  | Hz | 21 | 40 | 100 | 200 | 400 | 1k | 2k | 4k | 8k | 18.5k |
  |---|---|---|---|---|---|---|---|---|---|---|
  | R | .92 | .88 | .80 | .67 | .49 | .29 | .18 | .12 | .07 | .05 |
  | G | .39 | .46 | .58 | .71 | .80 | .79 | .68 | .53 | .41 | .32 |
  | B | .07 | .09 | .16 | .23 | .34 | .53 | .71 | .84 | .91 | .94 |
  | hue° | 23 | 28 | 40 | 65 | 100 | 149 | 183 | 205 | 216 | 222 |

  The hue rises smoothly with log-frequency and flattens at both ends, as gentle overlapping band responses would.
- **Additive blend, not a lookup from a single "centroid" frequency.** Mixtures land **off** the pure-tone curve. White noise is (0.30, 0.43, 0.85), bluer *and* redder than any pure tone. The 100 Hz square is desaturated (0.66, 0.57, 0.48).
- **Brightness in the stripe:**
  - It rises with frequency at equal level: ‖rgb‖ is 72 at 40 Hz and 227 at 16 kHz.
  - It scales with level roughly as **amplitude^0.25**: for 8 kHz, ‖rgb‖ is 227 at 0 dB, 187 at −6, 157 at −12, 134 at −18, 115 at −24 and 56 at −48. That is ×0.84 per −6 dB, and 0.5^0.25 = 0.84.
  - The hue does not change with level.
  - In the deck view, low tones are bright, e.g. a vivid red-orange at 40 Hz. So the deck's brightness rule differs from the stripe's.
- **The colour is smoothed in time; the shape is not.**
  - After a click or kick ends, the 1-px centre line keeps the sound's colour and decays.
  - In the stripe, the decay is ×0.67 per 35 ms column after a click (high band, τ ≈ 90 ms) and ×0.905 per column after a 60 Hz kick (low band, τ ≈ 350 ms). So release time is longer for lower bands.
  - The colour also builds up over ~1–2 columns after onset, while the height is instant.
  - In the deck view, a kick's coloured centre line also extends ~250–450 ms *before* the onset, as if the low band were analysed with a centred (non-causal) window. Clicks show no pre-onset line.
- **Silence:**
  - True digital silence draws a 1-px **black** centre line, darker than the background.
  - Low-level residue draws a 1-px line in the residual colour.

### 8.4 Mixxx vs Traktor, revised

| Aspect | Traktor (measured) | Mixxx today |
|---|---|---|
| Height | Linear signed min..max of the mono sum, per column | Per-band abs peak, high band `pow 0.632`, 441 slices/s, L/R halves |
| Detail | Sample-level: individual cycles visible at ~240 px/s | ≥2.3 ms slices, no sub-slice information |
| Body brightness | Sample density with additive clipping (hollow squares, bright sine edges, grain) | Solid fill |
| Colour | Smoothed (τ ≈ 90–350 ms, band-dependent) continuous spectral blend | Per-slice, unsmoothed, max-normalised |
| Silence | Black 1-px line | Axis line drawn always |

### 8.5 Prototype check against a real track (2026-10-17)

**Method.** `tools/PROTOTYPE_traktor_waveform.py` (throwaway, outside the build; now `tools/phosphor_waveform.py`) renders the §8 model from the decoded FLAC. It draws signed min..max of (L+R)/2, sample-density brightness with per-channel clipping, and a smoothed colour from three zero-phase filters fitted to the §8.3 table. The target is the deck B lane of `SCREENSHOTS/waveforms/Screenshot 2026-10-17 203136.png` (CYK "Gigando", 04:41). The result is `SCREENSHOTS/waveforms/PROTOTYPE_gigando_comparison.png`. Fits are per-column outlines and per-region mean colours measured against the screenshot. Evidence is **[V]** plus those fits.

- **NML `CUE_V2 START` is in milliseconds.** Gigando's AutoGrid starts at 1.16 ms, so bar 176 is at 281.60 s. Fitting the outlines independently puts the playhead at 281.59 s.
- **Zoom:** 136.1 px per track second (217.4 px per bar at 150 BPM), i.e. 7.35 ms per column.
- **Each column's min..max covers more than its own time.** A window centred on the column fits best at 16–30 ms (outline correlation 0.70, against 0.57 for the column's own 7.4 ms). By eye, 12 ms best matches the per-column streaks. This covers a whole kick cycle, so kicks look like filled, symmetric blobs. This refines §8.1's "no smoothing": the onset and offset stay sharp, but the window is wider than one column. Whether the window depends on zoom is **unknown**: at ~240 px/s individual 60 Hz cycles were visible.
- **Height:** full scale ≈ 0.8 × half the lane for this track. Its NML loudness is `PEAK_DB=0.30 PERCEIVED_DB=1.73`, so Auto-Gain could explain this (Q8 is still open).
- **The deck's colours are much more saturated than the stripe's.** Using the stripe's unit colour as-is renders near-white. A good match needed:
  - the unit colour raised to the 5th power and renormalised to max = 1;
  - blue ×1.8 and green ×0.85;
  - level compression `amplitude^0.35`;
  - colour attack 5 ms and high-band release 40 ms (the stripe gives 90 ms), with low and mid releases left at 350 and 180 ms.
- **Density light added to the background looked better on this screenshot.** This was **wrong** for Traktor's drawing: see the calibration below, which shows opaque pixels.
- **Result:** outline correlation 0.91, and mean height 31.2 vs 31.8 rows. The colour runs cyan → deep blue → green → yellow → orange through the breakdown, with orange bass and magenta kicks in the drop. Per-region mean colour is within ~10–25 levels.
- **Remaining gaps:** snares look less violet than Traktor's, and the breakdown is slightly too bright and green.
- **Caveat:** all tuned values come from this one screenshot and may be overfitted.

**Calibration against the test-signal screenshots** (`waveform-tests/`; the scripts were throwaway):

- **Deck colour of −6 dBFS tones (Q2a, 115 px/s)**, as the median of each tone's body (0–255; channels at 255 are clipped):

  | Hz | 40 | 63 | 100 | 160 | 250 | 400 | 630 | 1k | 1.6k | 2.5k | 4k | 6.3k | 10k |
  |---|---|---|---|---|---|---|---|---|---|---|---|---|---|
  | R | 79 | 124 | 126 | 99 | 70 | 43 | 24 | 21 | 15 | 8 | 4 | 1 | 0 |
  | G | 24 | 33 | 50 | 79 | 125 | 192 | 175 | 255 | 255 | 212 | 130 | 71 | 36 |
  | B | 15 | 13 | 13 | 14 | 20 | 24 | 28 | 79 | 191 | 255 | 255 | 255 | 255 |

  - This is a near fully saturated rainbow, far more saturated than the stripe's §8.3 colours.
  - Low tones are dim, while high tones clip.
- **Traktor draws covered pixels opaque.** The 40 Hz body has B = 15 and its edge rows are (11, 9, 0), both below the background's 24.
- **Height model confirmed at a second zoom:** the prototype gives 35 rows for a −6 dBFS tone at 115 px/s, against Traktor's 36.
- **Level compression on the deck is γ ≈ 0.87, not the stripe's 0.25** (Q3a, 100 Hz).
  - The body gets slightly *brighter* as the level falls: max channel 138, 143, 156, 171 and 200 at 0, −6, −12, −18 and −24 dBFS.
  - With density ∝ 1/amplitude, brightness ∝ A^(γ−1). The −6 vs −18 dB pair gives γ ≈ 0.87.
  - On the deck, a tone's brightness is therefore almost independent of its level.
- **Pure tones and music disagree under an additive band model.**
  - Filters fitted to the deck table above reproduce all 13 tones to within ~3 levels per channel.
  - On Gigando, the same model renders the drop yellow/beige: G ≈ 120–155 against Traktor's 66, and the intro is not blue enough.
  - An RMS colour envelope helps, and the strongest-STFT-bin envelope fixes the green, but blue stays too low.
  - The screenshot-tuned stripe model (above) still looks closest on music, even though its tones are ~30 levels off.
  - So in mixtures the dominant band wins more than adding tone colours predicts. **The mixing rule is still unknown.**
  - The variants are compared in `SCREENSHOTS/waveforms/PROTOTYPE_gigando_variants.png`.

**Second round (2026-10-18): two-tone mixtures (Q9) and four more tracks.** Source screenshots: `waveform-tests/Q9 two-tone mixtures/`, `SCREENSHOTS/traktor_screenshots/`.

- **Gain does not change the scrolling waveform** (user observation in Traktor).
  - There is no per-track normalisation either. All five tracks peak at 0.97–1.0 and their `PERCEIVED_DB` ranges from −2.3 to +14.5, yet the fitted full scale stays at ≈ 0.7–0.8 × half the lane.
  - So the height scale is fixed lane geometry, not gain.
- **Zoom is set per beat:** 217.4 px per bar for every track, whatever the BPM.
- **The height window scales with zoom.** At 513 px/s (Q9), Traktor draws the individual 100 Hz cycles as a zigzag, which a fixed 12 ms window cannot do. About 2–3 columns fits both zooms.
- **Q9 mixture colours** (median, 0–255):

  | segment | R | G | B |
  |---|---|---|---|
  | 100 Hz −12 | 216 | 79 | 8 |
  | 4 kHz −12 | 2 | 122 | 255 |
  | 100 + 4k, both −12 | 186 | 112 | 255 |
  | 100 −12, 4k −24 | 231 | 90 | 118 |
  | 100 −24, 4k −12 | 66 | 114 | 255 |
  | 100 + 1k, both −12 | 163 | 196 | 51 |
  | 1k + 4k, both −12 | 11 | 206 | 255 |

  - Green in a mix is close to the larger of the two tones' greens, not their sum. Simple addition would give ~200 for 100 + 4k.
  - Scaling one tone by −12 dB scales its own channels by ≈ 0.25^0.87 (R 216 → 66), which confirms the 0.87 level exponent.
  - No model tried (additive deck table, RMS, max-bin, stripe + saturation) fits all seven; the best is ~28 levels per channel. The exact mixing rule is still open.
- **Fix: split the level exponent.** The hue mix uses `levels^0.35` (from the screenshot fit), while brightness follows `level^0.87` (Q3).
  - With a single exponent of 0.35, quiet passages near the centre line blew out to white/yellow on every new track.
  - Scored on all five tracks (per-94-px mean colour plus centre-row brightness), the combined error fell from 30.2 to 16.7.
- **Polarity: the prototype had the waveform upside down.** Traktor draws positive samples *below* the centre line (Q5 click). The prototype now does the same.
- **Saturation, checked per pixel.** Median HSV saturation, the share of vivid pixels (S > 0.8) and median brightness were compared on all five tracks. Saturation 4 / gain 0.6 was too pale and too dark (−0.05 S, −0.08 vivid share, −0.11 V). Saturation 5 / gain 0.7 matches (+0.02, +0.01, −0.02), with the block-colour score almost unchanged (17.0 vs 16.7).
- **Current defaults:** stripe table, saturation 5, RGB gains 1/0.85/1.8, gamma 0.35, brightness gamma 0.87, density gain 0.7, additive blend, 12 ms window, height ×0.8, positive downward.
  - These look right on all five tracks without per-track tuning. Body colour error is 7–12 levels and centre-line error 9–20.
  - Remaining visible gap: yellow/white centre blobs between the kicks on dense tracks (*Progress Paradox*), where Traktor stays pink.
  - Sheets: `SCREENSHOTS/traktor_screenshots/PROTOTYPE_*_comparison.png`.

### 8.6 Reference script checks (2026-10-18)

`tools/phosphor_waveform.py` is the maintained reference script (the prototype promoted, see `docs/research/traktor-waveform-brief.md`, "Reproducing"). It uses the spec's decisions: a 2-column shape window, zoom in px per bar, linear-phase FIR colour filters and detail tiers. Both checks below ran on the five reference tracks (`--check-fir`, `--check-tiers`).

**FFT vs FIR colours.** Whole-track zero-phase FFT filtering vs the 4096-tap FIR (overlap-add, delay-compensated). Difference of the final colour of a density-1 pixel (colour x 0.7, 0-255, unclipped), over the whole track:

| Track | max R | max G | max B | mean R | mean G | mean B |
|---|---|---|---|---|---|---|
| Gigando | 0.47 | 0.29 | 0.37 | 0.104 | 0.059 | 0.043 |
| ren intro meaning | 0.09 | 0.05 | 0.07 | 0.016 | 0.004 | 0.003 |
| Braindrain intro | 0.31 | 0.20 | 0.12 | 0.073 | 0.020 | 0.010 |
| Spiritual | 0.66 | 0.42 | 0.39 | 0.113 | 0.064 | 0.046 |
| Progress Paradox | 0.55 | 0.38 | 0.52 | 0.160 | 0.090 | 0.061 |

The worst difference is 0.66 levels, under the 2-level criterion. The FIR filters keep the defaults.

**Detail tiers.** Pixel difference (0-255 per channel, whole lane) of each tier from Full, at the fitted play marker plus four more positions (20/40/60/80 % of the track): the mean over the five tracks and the largest single-pixel difference. Per-track means range from 0.3 to 13.3.

| Tier | N | Mean diff | Max diff |
|---|---|---|---|
| High | 4 | 2.41 | 106 |
| Medium | 8 | 4.51 | 166 |
| Low | 16 | 6.85 | 212 |
| Lowest | 32 | 9.35 | 227 |

Comparison images (Full above the four tiers, 4x around the play marker, per track): `docs/research/phosphor-detail-tiers/*.png`.

- The outline is identical by construction, as the images show. This holds because a shape window of 32 frames or more snaps its edges outward to multiples of 16 frames (the coarsest kept tier's block) in every tier, Full included, so all tiers see the same whole blocks; below 32 frames (deepest zoom) there is no snapping and tiers may differ. What changes is the texture inside the shape: single pixels flip between dim and bright, hence the large maxima.
- **High** and **Medium** look the same as Full at normal viewing size. Medium's texture is slightly grainier.
- **Low** has visibly more speckle on dense tracks (*Progress Paradox*, *Spiritual*). It is borderline.
- **Lowest** is visibly different: white speckle pixels in the density and a coarser texture. **Proposal: drop Lowest** (N = 32) from the list. Keep Full, High, Medium and Low. If Low looks wrong in the user's final manual check, drop it too.

## 9. Overview model fitted to Traktor's stored stripes (2026-10-18)

Ticket #20. `tools/phosphor_waveform.py --fit-overview --signals-dir <waveform-tests>` renders the Phosphor overview of the five reference tracks and the test signals and fits each to its stored stripe (`read_stripe`, `render_overview`, `fit_stripes`; tests in `tools/phosphor_waveform_test.py`). Traktor's folder was only read. The model constants were fitted on the four tracks with a stripe plus Q2a, Q2b, Q3c and Q4; Q6, Q7, Q9 and Q5 are checks.

**Matching a stripe to a track.** The header's block count is `N = ceil(frames / 1024)` of the track at 44.1 kHz, exactly (Gigando 21361, Spiritual 20144, Progress Paradox 20798, Braindrain 4962, and the test signals). **Each of the 1024 columns covers exactly N samples**: column k = samples `[k*N, (k+1)*N)`, and the columns after the end are empty. So stripes can be matched to tracks by N alone. *Ren intro meaning* (N = 4686) has no stripe in the folder (the stripes with N = 5100 are a different length), so it is not fitted. Traktor stored a single stripe for the three Q3 files (N = 1422); it is Q3c's, so Q3a/Q3b are skipped. Q4/Q6/Q7 and Q5/Q9 share a block count; the script assigns them by the best outline correlation.

### The model

| Part | Rule | Evidence |
|---|---|---|
| Rows | A sample `v` (mono, -1..1) falls in row `floor(31 v + 31)`, clipped to 0..61, positive downward. The rows from the min to the max of the column's **window** are present: the column's N samples plus N/2 on each side (ticket #58, below). | Top and bottom extents match the per-column min/max with r = 0.93-0.97 and a mean error of 0.7-0.9 row (Braindrain 2.2) on the four tracks. 0 dBFS gives 62 rows, -6 dB gives 32. |
| Colour | Unit colour of the column's band levels, taken as the **maximum over the column of the existing 1 ms colour track** (the same FIR filters, `table = stripe`, gamma 0.35 and smoothing as the scrolling waveform). Bytes are B, G, R: R = low band. | Mean distance between unit colours 0.053-0.081 on the tracks (cosine 0.995-0.997). Mean, middle or last block instead of the max is worse (0.10-0.14), so Traktor's column colour is a peak hold of the smoothed band levels. |
| Magnitude | `norm(rgb) = (W . (r, g, b, b^4)) * norm(levels)^0.7 * d^0.25`, with (r, g, b) the unit colour, W = (83.5, 242.0, -37.4, 606.1) and `d` the row's density (next row). | Per-pixel median error 11 % on the tracks and 10 % on tones. The 0.7 matches amplitude^0.25 for a pure tone (0.35 x 0.7 = 0.245). The `b^4` term lifts bright blue (high-frequency) columns, as pure tones need (a 16 kHz tone is about 3.5 times as bright as a 40 Hz tone). |
| **Profile** | `d` = the share of the column's samples that fall in the row (sample **density**), raised to 0.25. Rows inside the extent that no sample hit get 0.5 / N, a calibrated floor (see "Detail floor" under Remaining gaps). | Within a column, `log norm(rgb)` against `log d`: slope 0.26, R^2 = 0.91, residual 0.12 (67 000 pixels). This is the section 8.2 phosphor effect: Q7's sine is *brighter* at its edges (153 against 128), a square is a hollow box, and music columns are bright at the centre. The earlier "survival function" reading was close for unimodal columns but wrong for sines. |
| Alpha | `A = 2.0 x G byte` (median 2.02, 5 % error). | Fitted over 120 random stripes. **This corrects section 4.1**: alpha is *not* the norm of the colour (`c3 / norm(c0..c2)` ranges 0.84-1.55), and the premultiplied reading fails (8 kHz tone: c0 = 209 > c3 = 193). |

### Fit (final model)

Colour error is the mean distance between the columns' unit colours (0 = same, 1.41 = orthogonal, so 0.07 is about 4 degrees). Profile error is the mean absolute alpha difference over the pixels either image has (0-255). Outline r is the mean correlation of the first and last row. "Cols" are the columns where both have signal.

| Item | N | Cols | Colour err | Profile err | Outline r |
|---|---|---|---|---|---|
| Gigando | 21361 | 1024 | 0.072 | 8.9 | 0.971 |
| Braindrain intro | 4962 | 1024 | 0.081 | 18.5 | 0.928 |
| Spiritual | 20144 | 1024 | 0.053 | 9.1 | 0.950 |
| Progress Paradox | 20798 | 1024 | 0.066 | 6.2 | 0.967 |
| Ren intro meaning | 4686 | no stripe | | | |
| Q2a crossover steps | 3058 | 1011 | 0.065 | 36.1 | 0.955 |
| Q2b sweep | 2671 | 1009 | 0.021 | 29.1 | 0.970 |
| Q3c 8 kHz steps | 1422 | 994 | 0.143 | 62.7 | 0.995 |
| Q4 sine vs square | 905 | 978 | 0.070 | 24.5 | 0.987 |
| Q6 sine vs noise | 905 | 978 | 0.099 | 34.7 | 0.989 |
| Q7 stereo | 905 | 748 | 0.068 | 13.6 | 0.991 |
| Q5 clicks and kicks | 1551 | 522 | 0.181 | 79.7 | 0.758 |
| Q9 two-tone mixtures | 1551 | 997 | 0.079 | 29.7 | 0.983 |

Means over the four tracks: colour 0.068, profile 10.7, outline 0.954. The comparison image is `docs/research/phosphor-overview-fit.png` (Traktor above, ours below, per item; 64 colours to keep it small).

### How it differs from the scrolling waveform

- One column spans **N samples (0.2-0.5 s for a track)**, not a pixel, and there is no shape smoothing across columns.
- Brightness comes from the **density raised to 0.25**, not linear density added up and clipped. That is why the overview is smooth and the scrolling waveform grainy.
- Colour is the **peak hold over the column** of the same smoothed band levels; the scrolling waveform samples the levels per pixel.
- Magnitude depends on hue (blue is brighter) and on `norm(levels)^0.7`; the scrolling waveform's brightness is its own fit (section 8.5).
- Alpha is a separate channel (2 x the green byte). The Phosphor renderer can use the colour alone. The widget draws the colour opaque (`overviewDisplayImage`), because premultiplying by that alpha darkens the colour a second time (red and blue columns, low G, nearly vanish) while the model was fitted against the RGB as is.

### Remaining gaps

1. **Colour tails: fixed (ticket #57).** After a click or kick Traktor's 1-pixel centre line keeps its colour for 40+ columns, fading from blue to pure red, as the high band falls first and the low band last. The stored levels now release across columns: `level[k] = max(level[k], level[k-1] * exp(-column seconds / tau))`, with tau = 0.7 s for low, 0.1 s for high and none for mid (`hold_levels`, `OVERVIEW_HOLD`; applied to the stored bytes, only within the built columns). Grid search over the three taus on Q5/Q3c with the four tracks as a guard: a mid hold only hurt (Q3c); low above 0.7 s and high above 0.12 s helped Q5 further but began to move the music numbers or overshoot Q3c, so the values stay at 0.7/0.1. On N = 20 000 tracks (0.45 s columns) the hold is a small effect. Colour error before to after (this machine's signals): Q5 0.202 to 0.090, Q3c 0.136 to 0.055, Q2a 0.066 to 0.032, Q4 0.072 to 0.044, Q6 0.099 to 0.070, Q7 0.052 to 0.025, Q2b 0.021 to 0.015, Q9 0.080 to 0.053; the four tracks are unchanged (0.072/0.081/0.053/0.066, profile and outline too). Q7's outline r moves 0.9936 to 0.9929 (fewer columns compared). Still open: Q5's outline r (0.76) and profile error (84), because Traktor's tail is an opaque (alpha 255) one-row line where ours is faint. The version became `PhosphorOverview-1.1`.

   **Silence line (ticket #59, fixed).** In every stripe with digital silence (Q4, Q5, Q6, Q7, Q9: 9-233 silent columns each) Traktor's silent column is a single opaque black pixel `(0, 0, 0, 255)` at row 30, where a sample of 0 falls in row 31 of the map; so the map stays and only the line moves. The model now draws a silent column (`norm == 0`) one row above its extent, i.e. at row 30. Profile error fell on all eight signals with silence (Q7 21.7 to 13.1, Q5 92.6 to 71.6, Q3c 63.3 to 62.3, others by 0.1-0.7); colour error and the four tracks (no silence) are unchanged. Outline correlation moved by -0.0004 to -0.0055 on five signals (Q5 0.7585 to 0.7530): Pearson noise on the silent columns where Traktor has a colour tail (gap 1 above), not a worse extent. Stored data is unchanged, so no version bump. Review fix: silence is judged from the stored audio, not the gained levels. A column is silent when its three level bytes are 0 and its extent is the single row 31; with all gains at 0 a loud column is just dark, not a black block.

   **Detail floor (review of #58).** Rows inside the extent that no sample hit get 0.5 / N. Since #58 density is a share of a window of about 2N samples, so this is about one sample, not half. Setting it to half a sample of the window (0.25 / N) moved the four tracks by at most 0.01 (Gigando 8.20 to 8.19, Braindrain 15.97 to 15.96, Spiritual and Progress Paradox unchanged) but made Q4 worse (24.4 to 28.1) and Q2a slightly (35.9 to 36.4). It stays 0.5 / N as a deliberate calibrated floor.
2. **Tone profile** (investigated in #58, not fixed: see below). A 62-row tone has dim edges in Traktor (152 against 193) over about 5 rows; our density model gives a flat or edge-bright profile. Traktor probably draws the segments between samples and anti-aliases the edge (section 8.2). Profile errors on the test signals (14-80) are mostly this and the one-row offset. On music they are 6-9, except Braindrain (18, a heavily compressed master).
3. **Brightness model.** 11 % median per pixel; the `b^4` term and the negative blue weight are a fit, not a mechanism.
4. **Ren intro meaning** has no stripe, so the five-track set is four.
5. ~~**Edge extents** are 0.7-0.9 row wider than min..max~~ Fixed in #58: Traktor's extent comes from a two-column window.

### Ticket #58: line segments, anti-aliased edges and the extents (2026-10-20)

Tested in the reference script (`--overview-overlap`, `--overview-segments`); the baseline is the table above re-run on the current signals.

**Extents: the cause is a wider window, not anti-aliasing.** Traktor's top and bottom rows on music are 0.7 row (Gigando, Spiritual), 0.4 (Progress Paradox) and 2.1 rows (Braindrain) beyond the column's own min and max, on each side, by the same amount on both. Extending each column's window by a fraction of a column on each side and measuring the signed bias: 0.25 N leaves 0.3 (Braindrain 0.8); **0.5 N on each side (a window of two columns, the same two-column shape window as the scrolling waveform, section 8) leaves 0.00-0.05 on all four tracks**; 1.0 N overshoots by the same amount the other way. Tones have constant amplitude, so they show no bias. Anti-aliasing the edge row cannot explain it: it would also widen the tones. Adopted for the extent and the density: both come from the window (N + 2 x floor(N/2) samples, clipped to the track). A column whose own samples are all zero keeps its one-row line.

**Tone edges: line segments reproduce the ramp, but not the brightness.** In Q3c (8 kHz, full scale; 62 rows) Traktor's alpha is flat (193) with a ramp over 5 rows at each edge (152, 168, 177, 185, 191, 194; as `d`, relative: 0.38 0.57 0.71 0.84 0.96). Counting a segment between consecutive samples **once in every row it crosses** (a drawn line; spreading a unit of weight over its rows gives a lumpy profile instead) gives 0.43 0.63 0.76 0.88 0.98 and a flat interior; at 16 rows the edge is 179 against 197 in Traktor and 0.71 against 0.91 in `d`. But the crossing share is far larger than the point share for high frequencies (0.37 against 0.012 in the interior), so the calibrated brightness (exponent 0.25, W) is then wrong by 1.5-2.4 times for tones and has to be refitted. The best compromise (exponent 0.25, brightness 0.85) lowers the tone mean profile error from 39.8 to 37.5 (Q2a 36 to 29, Q3c 63 to 52, Q6 35 to 28) but raises it on the sine tones (Q4 25 to 28, Q7 21 to 25) and on Progress Paradox (5.5 to 6.1); nothing dominates. **Not adopted** (the option stays in the script, off). The edge ramp is a drawn-line effect; a refit of the magnitude model on counts of crossed rows, not shares of samples, is the next step if the tone profile matters.

**Fit after adopting the window** (profile error / outline r; colour is unchanged; before in brackets):

| Item | Profile | Outline r |
|---|---|---|
| Gigando | 8.2 (8.9) | 0.977 (0.971) |
| Braindrain intro | 16.0 (18.5) | 0.962 (0.928) |
| Spiritual | 8.2 (9.1) | 0.981 (0.950) |
| Progress Paradox | 5.5 (6.2) | 0.981 (0.967) |
| Q2a | 36.0 (36.1) | 0.957 (0.956) |
| Q2b | 29.1 (29.1) | 0.970 (0.973) |
| Q3c | 63.0 (63.3) | 0.998 (0.995) |
| Q4 | 24.7 (24.8) | 0.987 (0.988) |
| Q6 | 35.0 (35.1) | 0.988 (0.989) |
| Q7 | 21.8 (21.7) | 0.994 (0.994) |
| Q5 | 86.7 (92.6) | 0.791 (0.759) |
| Q9 | 29.8 (29.9) | 0.983 (0.984) |

The table is before the colour tails (#57) and silence line (#59) merged; with all three, the profile errors are Q2a 35.9, Q2b 29.0, Q3c 62.0, Q4 24.4, Q6 34.3, Q7 13.1, Q5 67.5, Q9 29.5 and the four tracks as above.

Mean width bias of the extent on the four tracks: from -0.76..-1.40 rows (Braindrain -4.2) to -0.11..+0.03 (Braindrain -0.03). The tone profile error does not change; it stays dominated by the brightness scale and the one-row offset. The stored layout is unchanged; the overview version is 1.1.

### Q1: band data or a rendered image?

**Not settled.** Settling it needs the same track analysed in two colour modes. The Traktor folder is read-only, Traktor is not run here, and all three stripe folders were written in mode 3. What the fit adds: a stripe column is exactly {extent, 3 band peak levels, a density profile}, with the colour a fixed function of the band levels and the profile independent of the colour; that is enough to recolour at draw time (reading 2). The alpha is 2 x G, not the colour's norm, which a plain rendered premultiplied image (reading 1) would give. So reading 2 (mode-neutral data plus a renderer) is somewhat more likely, but only the Q1 experiment proves it. Our design does not depend on the answer: it stores band levels and density, not an image.

### Proposed stored format for the overview analysis type (for #21)

Store what the model needs, not an image, so the overview can be redrawn at any width and recoloured. Split the track into **W columns of N = ceil(frames / W) samples** (W = 1024 as Traktor, or more for wide screens; the model is per column). Per column:

| Field | Type | Meaning |
|---|---|---|
| `levels[3]` | 3 x u8 (u16 for parity with the scrolling colour track) | peak hold over the column of the 1 ms colour track (low, mid, high), `round(level * 255)` |
| `density[62]` | 62 x u8 | `round(255 * (d / 0.8)^0.25)` clipped, where d is the share of the column's samples in the row; row 0 = -1.0 .. row 61 = +1.0, positive downward. Zero outside the extent. |

That is 65 bytes per column, **66 560 bytes (65 KB) per track at 1024 columns**, before compression (the density rows are smooth, so zlib should give 2-3 times less). The extent is implied by the first and last non-zero density row; the renderer adds the half-sample floor for gaps inside it. A leaner variant stores `min`, `max` and 8 quantiles of the sample distribution per column (11 bytes, 11 KB per track) and rebuilds the profile from them: exact for unimodal columns, but it loses the hollow-box and edge-bright shapes of tones and clipped masters. Recommendation: the 65-byte version, with the lean one as the fallback if size matters.

## 10. On-screen colours of the overview (2026-10-20, ticket #60)

Traktor does not draw the stored stripe as is. On screen its overview is vivid (hot pink and red for music, saturated hues for tones) and each column fades from full colour at the centre to almost the background at its edges; the stored stripe is dim and desaturated. `--fit-screen --signals-dir <waveform-tests>` compares them per pixel over the whole widget (Q2a, Dilema, Gigando).

**Geometry (measured from the background colour, not guessed).** The deck overview is 672 px wide (x0 = 42, or 41 in the older Gigando shot) and 33 rows tall (Q2a's shots: 51 rows), 33-grey between 24-grey borders. Stored columns map to screen x by area (1024 -> 672), the 64 stored rows to the widget rows by area average; the alignment is exact at shift 0 (error doubles at +-2 px).

**First attempt (7ae4adbadd) and why it failed.** It was fitted on each column's centre row only and flattened the column toward its brightest pixel (`p (peak / |p|)^0.8`), which removed the profile: edge / centre 0.85 against Traktor's 0.45 (B 255 at the centre, about 30 at the edges at Dilema's x = 500), so loud sections were slabs filling the height.

**Mapping** (per pixel p = stripe colour R, G, B, 0..255; `screen_colour`, `screen_cover`, `overview_display` in the script; `overviewDisplayImage` in C++):

1. colour: `q = p - 0.32 min(p)`; `pre = 114 * (1, 0.79, 1.07) * (q / 255)^2.5 * 255` per channel; `colour = pre / max(1, max(pre) / 255)` (hue kept past full value).
2. coverage (alpha): `(3.6 max(p) / 255)^2.18`, clipped to 1. The faint rows at a column's edge blend into the background; the density profile of the stored data is the profile on screen.
3. A pixel with alpha but no colour (the silence line) stays opaque black; an empty pixel stays empty.

Fitted by robust least squares per pixel over the whole widget on Q2a, Dilema and Gigando (ours as stored, background 33). Physical reading: the on-screen column = boosted colour over the background, weighted by how much of the stored pixel is there. Fitting on Traktor's real stored stripes instead gives the same structure with lower error (Dilema 10.8, Gigando 5.9, Q2a 6.1 with alpha from the stored alpha), so the remaining error on ours is the stored model's hue and level, not the mapping. Alpha from the brightest channel (not the stored 2 x green) is kept because a red-only or blue-only pixel (low-only or high-only audio) has stored alpha 0 and must still show. The stored data and `kOverviewVersion` are unchanged; Brightness, exponents and gain still shape the pixel the mapping starts from.

**Result** (per-pixel mean absolute error 0-255 over the widget, max(R,G,B) at the centre rows and 2 rows inside Traktor's drawn extent, loud columns right of the playhead):

| | Traktor centre / edge / ratio | 7ae4adbadd: error, centre / edge / ratio | now: error, centre / edge / ratio |
|---|---|---|---|
| Dilema | 240 / 110 / 0.46 | 34.7, 204 / 176 / 0.86 | 10.9, 239 / 115 / 0.48 |
| Gigando | 234 / 106 / 0.45 | 32.3, 198 / 168 / 0.85 | 9.2, 224 / 98 / 0.44 |
| Q2a | 215 / 239 / 1.11 | 10.7, 246 / 246 / 1.00 | 9.8, 190 / 218 / 1.15 |

In the app (Normalize on, visual gain 1, resampled to Traktor's geometry) edge / centre is 0.46 / 0.45 against Traktor's 0.45 and centre 224 / 223 against 225 / 234 (Dilema / Gigando). Still off: Dilema's magenta reads (225, 82, 202) in Mixxx against Traktor's (175, 126, 255) (the stored model's blue is low there) and Gigando is a little orange; these are stored-colour differences.

**Not done / open.** The stored model's hue on magenta and warm sections; the playhead-side darkening of played audio on Traktor's screen.

## Implications for a Traktor-style renderer in Mixxx

> **Superseded in part by §8.** The list below was written before the tests. Its smoothed, mirrored envelope (items 2–3) and fixed gradient (item 5) do **not** match Traktor. See "Revised plan" at the end of this section.

**Achievable now (renderer only, no re-analysis).** Build a new allshader renderer (base it on `waveformrendererrgb.cpp`, not the textured one):

1. Per pixel, gather all slices in range. Use `unscale()` on high (and on all, if used) to get back to linear peaks.
2. **Smooth over time:** per band, instant attack with exponential release, or a short moving average over neighbouring slices. #16963 and #17141 both found attack/release essential.
3. **Height:** symmetric `max(L, R)` envelope, no ×2 compensation, and a user-tunable curve (start linear).
4. **Colour:** `rgb = (low·Clow + mid·Cmid + high·Chigh) / ‖(low,mid,high)‖₂` with Clow = red, Cmid = green, Chigh = blue for a "Spectrum" preset. Smooth the colour too.
5. **Brightness gradient:** alpha falls from ~1.0 at the centre to ~0.35 at the envelope edge, matching the stripe profile in §4.1. Optionally modulate it by how many of the pixel's slices reach each height, which gives a cheap version of the grain.
6. No axis line. Draw one smooth shape (triangle strip) rather than per-pixel rectangles.

**Needs analyzer changes (new waveform version, re-analysis):**
- RMS per band alongside peak (#14325) for Traktor-like loud/quiet contrast.
- A higher slice rate, signed min/max, or a small per-slice amplitude histogram for a true density texture.
- Configurable or sharper crossovers (#17175), if visual tests show Traktor's bands sit elsewhere.
- Removing the `pow` from storage altogether, which is cleaner than undoing it at render time.

**Open questions the user can settle visually** (generate test files, analyse them in Traktor, and compare its screen with our stripe parser):

- **Q1, colours vs bands in stripes:** analyse one track in Spectrum, copy its stripe file, switch to X-Ray, force re-analysis, and diff the files. If they are identical, the stripe holds band data and the renderer recolours it. If they differ, the stripe is a rendered image.
- **Q2, crossovers:** sine sweep 20 Hz–20 kHz, or stepped tones at 50, 100, 200, 400, 800 Hz and 1.6, 3.2, 6.4, 12.8 kHz. Note where the hue changes from red to green to blue, and where it is mixed.
- **Q3, amplitude scaling:** a 1 kHz tone stepped through 0, −6, −12, −18, −24, −36 dBFS. Measure the waveform height (or the stripe's non-zero rows) per step. A linear scale halves the height per −6 dB, a log scale steps it evenly, a sqrt scale drops it by ~0.71 per step.
- **Q4, peak vs RMS:** a square wave and a sine at equal peak level. They look the same height if Traktor draws peaks, and the square looks ~1.41× taller if it draws RMS.
- **Q5, smoothing:** an isolated click every beat. Measure how many pixels the blob extends before and after the click at a known zoom (attack/release time).
- **Q6, grain:** pure sine (steady) vs white noise at equal level. A density rendering would show a smooth gradient for the sine (arcsine-like distribution) and speckle for the noise. A fixed dither texture would look the same on both.
- **Q7, symmetry and stereo:** a hard-panned L-only signal. It shows whether Traktor's halves are L/R or mirrored mono.
- **Q8, gain:** move the gain knob and Auto-Gain and check whether the scrolling waveform height changes.

Q2–Q7 were answered on 2026-10-17 (§8). Q1 is not settled by the stripe fit (§9) and needs the colour-mode experiment; Q8 is still open.

**Revised plan (after §8).**
1. **Shape and density need sample-level data that Mixxx's waveform does not keep.**
   - Two options: (a) build a mono sample buffer (or a high-rate min/max + histogram summary) at track load, like Traktor's apparent load-time `WaveformDataGenerator`; or (b) add a new analysis representation, for example 1 ms sub-slices with signed min/max and a small amplitude histogram.
   - Either way, the renderer draws per column the signed min..max of the mono sum, with brightness `clamp(colour · k · hits)` per channel.
2. **Colour is a separate, low-rate, smoothed signal.**
   - Band envelopes come from gentle overlapping filters (Mixxx's existing low/mid/high would do as a start).
   - Smooth them with band-dependent release (~90 ms high, ~350 ms low) and a short attack.
   - Blend additively into RGB, fitted to the §8.3 table, and keep brightness modest.
   - This part could start from Mixxx's existing 441 Hz band data.
3. **Details:** black 1-px centre line in silence, no axis line, linear scale with full scale = lane height.
4. **From the prototype (§8.5):**
   - Take min..max and density over a window of ~2–3 columns centred on each column, not over the column's own time.
   - Make the deck's colour saturation, per-channel gains and band release times tunable rather than copying the stripe's numbers.
   - Keep separate level exponents for hue (~0.35) and brightness (~0.87).
   - Leave the waveform height independent of gain and of track loudness.

---

## Sources

**Native Instruments (primary)**
- Traktor Pro manual, Basic Usage Tutorials: https://docs.native-instruments.com/online-guides/traktor-pro-manual/en/basic-usage-tutorials
- Traktor Pro manual, Preferences: https://docs.native-instruments.com/online-guides/traktor-pro-manual/en/preferences
- NI-shipped QML in the local install, `C:\Program Files\Native Instruments\Traktor Pro 4\Resources64\qml\`: `Screens/Defines/Colors.qml:414-497`, `Screens/S8/Views/Waveform/SingleWaveform.qml:25-38`, `Screens/S8/Views/Waveform/Stripe.qml:7-23`, `Screens/S8/Views/Waveform/WaveformColorize.qml:59-63`, `Screens/S8/Views/Waveform/StemWaveforms.qml`, `CSI/Common/Pioneer/Waveform.qml:19`
- Strings in `Traktor Pro 4.exe` (4.0.2): mode names; `Base\Utils\Waveform\WaveformDataGenerator.cpp`; `updateStripeAndTransientsFromTrack`; `onWaveformAnalyseCompleted`. Only plain strings were read. Nothing was disassembled.

**Reverse engineering / data inspection**
- Our inspection of `Documents\Native Instruments\Traktor 4.0.2\{Stripes,Transients,collection.nml,Traktor Settings.tsi}` (plus the 3.5.0 and 3.10.0 folders), using throwaway Python scripts. The method and minimal reader are in §4.1.
- hellricer, "Decoding the TRAKTOR4 field" (2021): https://hellricer.github.io/2021/05/05/decoding-traktor4-field.html
- kokernutz/traktor-kontrol-screens: https://github.com/kokernutz/traktor-kontrol-screens (`Screens/Defines/Colors.qml`)
- ErikMinekus/traktor-kontrol-screens: https://github.com/ErikMinekus/traktor-kontrol-screens
- wolkenarchitekt/traktor-nml-utils: https://github.com/wolkenarchitekt/traktor-nml-utils
- Deep-Symmetry crate-digger, Rekordbox ANLZ spec: https://github.com/Deep-Symmetry/crate-digger/blob/main/src/main/kaitai/rekordbox_anlz.ksy

**Mixxx (this repo, `djnote` branch)**
- `src/analyzer/analyzerwaveform.cpp:13-15, 59-61, 156-158, 196-234`
- `src/analyzer/analyzerwaveform.h:20-39, 73-108`
- `src/waveform/waveform.h:14-28`; `src/waveform/waveform.cpp:44-63`
- `src/waveform/renderers/allshader/waveformrendererrgb.cpp:78, 117-199`; `waveformrendererfiltered.cpp:64, 108-186`; `waveformrendererhsv.cpp:138-159`; `waveformrenderersimple.cpp:68, 116-139`; `waveformrendererlrrgb.cpp:163-192`; `waveformrenderertextured.cpp:192, 305, 414-415, 475-476`
- `res/shaders/rgbsignal.frag:34-122`; `res/shaders/stackedsignal.frag:65-145`
- `src/waveform/renderers/waveformrenderersignalbase.h:30-37`; `waveformrenderersignalbase.cpp:172-228`
- `src/waveform/renderers/waveformwidgetrenderer.h:115-129`; `waveformwidgetrenderer.cpp:96`; `src/engine/enginepregain.cpp:118-121`
- `src/waveform/widgets/waveformwidgettype.h` (type numbers); `res/skins/LateNight/skin.xml:187-189, 327-330` (RGB band colours)

**Mixxx GitHub**
- PRs: #13075 https://github.com/mixxxdj/mixxx/pull/13075 · #14325 https://github.com/mixxxdj/mixxx/pull/14325 · #16963 https://github.com/mixxxdj/mixxx/pull/16963 · #17141 https://github.com/mixxxdj/mixxx/pull/17141 · #17175 https://github.com/mixxxdj/mixxx/pull/17175 · #17129 https://github.com/mixxxdj/mixxx/pull/17129 · #15516 https://github.com/mixxxdj/mixxx/pull/15516 · #14998 https://github.com/mixxxdj/mixxx/pull/14998
- Issues: #11833 https://github.com/mixxxdj/mixxx/issues/11833 · #6352 https://github.com/mixxxdj/mixxx/issues/6352
- audio-viz crate: https://crates.io/crates/audio-viz

**Secondary / anecdotal (labelled as such above)**
- crossfadr, "What's new in Traktor Pro 2" (2011): https://crossfadr.com/2011/06/21/whats-new-in-traktor-pro-2/
- Resident Advisor, Traktor Pro 2 review: https://ra.co/reviews/8962
- DJ TechTools forum, "Read traktor stripe files" (2010): https://forum.djtechtools.com/showthread.php?t=22289
- Digital DJ Tips, "DJ Software Secrets": https://www.digitaldjtips.com/dj-software-secrets/
