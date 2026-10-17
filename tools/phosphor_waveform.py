# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow"]
# ///
"""Phosphor waveform reference script.

The maintained reference implementation of DJNote's Phosphor waveform, promoted
from the throwaway Traktor prototype (branch prototype/traktor-waveform). The
C++ code is checked against it. Model and calibration:
docs/research/traktor-waveform-brief.md and docs/research/traktor-waveform.md
(section 8 and 8.5 win). Terms: GLOSSARY.md.

What it does
------------
- Render one deck lane of a real track (default mode) and, with --compare, put
  it next to a Traktor screenshot. The prototype's options (--table,
  --envelope, --compare, ...) still work.
- --check-fir: FFT vs FIR colour filters on the five reference tracks.
- --check-tiers: Waveform detail tiers vs Full on the five reference tracks.
- --fixtures DIR: write the deterministic fixtures for the C++ tests
  (format: src/test/phosphor/README.md).

Model, in this script's defaults (the calibrated values)
--------------------------------------------------------
- Samples: mono (L+R)/2 as int16 at the file's rate.
- Shape: rows y = centre + m * half, positive downward, half = 0.8 * lane
  height / 2. Window: --shape-window columns (2) centred on the column, never
  fewer than the two points around the column centre.
- Zoom: px per bar = 869.6 / z (217.4 at z = 4); a bar is 4 beats at the
  track's BPM (120 if none): px/s = px_per_bar * bpm / 240.
- Colour: three linear-phase FIR filters (4096 taps at 44.1 kHz) with
  g_c(f) = unit_table_c(f)^(1/0.35), overlap-add, delay-compensated; peak per
  1 ms, ^0.35, one-pole smoothing (attack 5 ms, release 350/180/40 ms, red
  also max'ed with a backward 120 ms follower). --filter fft gives the
  prototype's whole-track zero-phase FFT filtering.
- Detail tiers: --detail full/high/medium/low/lowest = block N 1/4/8/16/32.
  Each block keeps its min and max in the order they occurred.

Usage (needs ffmpeg on PATH):
    uv run tools/phosphor_waveform.py "<track.flac>" --time 281.6 \
        --px-per-sec 136.1 --compare "<traktor screenshot.png>"
"""

import argparse
import os
import struct
import subprocess
import sys
import wave
from dataclasses import dataclass

import numpy as np
from PIL import Image, ImageDraw

RATE = 44100
SILENCE, SIGNAL, OUTSIDE = 2, 1, 0
TIERS = {"full": 1, "high": 4, "medium": 8, "low": 16, "lowest": 32}

# Section 8.3: unit colour of pure tones at -6 dBFS in Traktor's stripe.
TABLE_HZ = np.array([21, 40, 100, 200, 400, 1000, 2000, 4000, 8000, 18500], float)
TABLE_RGB = np.array([
    [.92, .88, .80, .67, .49, .29, .18, .12, .07, .05],
    [.39, .46, .58, .71, .80, .79, .68, .53, .41, .32],
    [.07, .09, .16, .23, .34, .53, .71, .84, .91, .94],
])

# Deck colour (0-255) of -6 dBFS tones in Traktor's scrolling waveform, median
# of each tone's body in waveform-tests/Q2a crossover steps/*.png (115 px/s).
# Channels at 255 are clipped, so the true value there is higher.
DECK_HZ = np.array([40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000,
                    6300, 10000], float)
DECK_RGB = np.array([
    [79, 124, 126, 99, 70, 43, 24, 21, 15, 8, 4, 1, 0],
    [24, 33, 50, 79, 125, 192, 175, 255, 255, 212, 130, 71, 36],
    [15, 13, 13, 14, 20, 24, 28, 79, 191, 255, 255, 255, 255],
]) / 255
TEST_TONE_LEVEL = 0.5  # -6 dBFS

# Traktor screenshot geometry for deck B (Screenshot 2026-10-17 203136.png).
SHOT_LANE = (31, 169, 1913, 256)  # left, top, right, bottom (exclusive)
SHOT_PLAYHEAD_X = 972
BACKGROUND = (24, 24, 24)

# The five reference tracks: file, track time (s) under the play marker and
# px/s fitted against the Traktor screenshots (prototype commit message).
TRACKS_DIR = r"G:\Music\DESKTOP MUSIC DJ SETS\UPDATED TRACKLIST"
TRACKS = [
    ("gigando", "01 - Gigando.flac", 281.6, 136.1),
    ("ren_intro", "ren intro meaning.flac", 27.328, 135.88),
    ("braindrain", "BRAINDRAIN-REMEMBER INTRO 24bMSTR.wav", 45.782, 137.69),
    ("spiritual", "15. Alien Coffee, Karev - Spiritual.flac", 190.030, 139.50),
    ("progress_paradox", "03 - Progress Paradox.flac", 440.214, 137.69),
]


# ---------------------------------------------------------------- zoom


def px_per_bar_for_zoom(z):
    """Deck zoom control value -> px per bar (z = 4 gives 217.4)."""
    return 869.6 / z


def px_per_sec(px_per_bar, bpm):
    """A bar is 4 beats at the track's BPM, 120 if it has none."""
    bpm = bpm if bpm > 0 else 120.0
    return px_per_bar * bpm / 240.0


# ---------------------------------------------------------------- audio


def decode(path, rate=RATE):
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2",
         "-ar", str(rate), "-"],
        check=True, capture_output=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).astype(np.float64)


def mono_int16(left, right):
    """(L+R)/2 as int16. Inputs are int16 or floats in [-1, 1); the sum is
    taken in units of int16 steps and rounded half up."""
    if np.issubdtype(np.asarray(left).dtype, np.floating):
        left, right = np.asarray(left) * 32768.0, np.asarray(right) * 32768.0
    s = (np.asarray(left, np.float64) + np.asarray(right, np.float64)) / 2
    return np.clip(np.floor(s + 0.5), -32768, 32767).astype(np.int16)


def build_tier(mono, n):
    """Points (times, values) of the detail tier with block size n. n = 1 is
    every sample. Otherwise each block keeps its min and max in the order they
    first occurred, placed at 1/4 and 3/4 of the block's length (a partial last
    block uses its own length)."""
    mono = np.asarray(mono, np.int16)
    if n == 1:
        return np.arange(len(mono), dtype=np.float64), mono.copy()
    full = len(mono) // n
    times, values = [], []
    if full:
        b = mono[:full * n].reshape(full, n)
        imin, imax = b.argmin(1), b.argmax(1)
        lo, hi = b.min(1), b.max(1)
        first = np.where(imin <= imax, lo, hi)
        second = np.where(imin <= imax, hi, lo)
        k = np.arange(full) * n
        times.append(np.stack([k + n * 0.25, k + n * 0.75], 1).reshape(-1))
        values.append(np.stack([first, second], 1).reshape(-1))
    rest = mono[full * n:]
    if len(rest):
        imin, imax = int(rest.argmin()), int(rest.argmax())
        lo, hi = rest.min(), rest.max()
        pair = (lo, hi) if imin <= imax else (hi, lo)
        k = full * n
        times.append(np.array([k + len(rest) * 0.25, k + len(rest) * 0.75]))
        values.append(np.array(pair, np.int16))
    return np.concatenate(times), np.concatenate(values).astype(np.int16)


# ---------------------------------------------------------------- colour


def channel_responses(n_fft, gamma, table, saturation, clip_boost=1.0, rate=RATE):
    """Linear magnitude response per colour channel on the rfft grid of n_fft
    points, fitted so that a pure tone gives channel levels proportional to
    amplitude**gamma * table colour. The stripe table is unit colour only
    (g_c = unit_table_c ** (1/gamma)). The deck table keeps its brightness,
    and its unit colour is pre-desaturated by 1/saturation so that, after
    deck_colour() re-saturates it, a -6 dBFS tone reproduces the table
    exactly; saturation then only changes how mixtures blend."""
    hz, colours = (DECK_HZ, DECK_RGB) if table == "deck" else (TABLE_HZ, TABLE_RGB)
    if table == "deck":
        # Clipped entries only give a lower bound; scale them up.
        colours = np.where(colours >= 1, colours * clip_boost, colours)
    freqs = np.fft.rfftfreq(n_fft, 1 / rate)
    logf = np.log(np.clip(freqs, hz[0], hz[-1]))
    rgb = np.stack([np.interp(logf, np.log(hz), c) for c in colours])
    norm = np.linalg.norm(rgb, axis=0)
    unit = (rgb / norm) ** (1 / saturation) if table == "deck" else rgb / norm
    unit /= np.linalg.norm(unit, axis=0)
    if table == "deck":
        return (unit * norm) ** (1 / gamma) / TEST_TONE_LEVEL
    return unit ** (1 / gamma)


def fir_length(rate):
    """About 4096 taps at 44.1 kHz, scaled with the sample rate, even."""
    return int(round(4096 * rate / 44100 / 2)) * 2


def fir_taps(rate, gamma=0.35, table="stripe", saturation=5.0, clip_boost=1.0):
    """Linear-phase FIR taps, shape (3, M): frequency sampling of the channel
    responses on an M-point grid, zero-phase impulse response rolled to the
    centre (delay M/2) and Hann-windowed."""
    m = fir_length(rate)
    g = channel_responses(m, gamma, table, saturation, clip_boost, rate)
    h = np.roll(np.fft.irfft(g, m, axis=1), m // 2, axis=1)
    window = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(m) / m)
    return h * window


def fir_filter_channels(x, rate, **design):
    """The three channel-filtered signals by FFT overlap-add, delay-compensated
    so they are zero-phase like fft_filter_channels."""
    taps = fir_taps(rate, **design)
    m = taps.shape[1]
    chunk = 32768
    size = 1 << int(np.ceil(np.log2(chunk + m - 1)))
    spectra = np.fft.rfft(taps, size, axis=1)
    n = len(x)
    out = np.zeros((3, n + size))
    for i in range(0, n, chunk):
        y = np.fft.irfft(np.fft.rfft(x[i:i + chunk], size)[None] * spectra, size, axis=1)
        out[:, i:i + size] += y
    return [out[c, m // 2:m // 2 + n] for c in range(3)]


def fft_filter_channels(x, rate, gamma=0.35, table="stripe", saturation=5.0, clip_boost=1.0):
    """The prototype's whole-track zero-phase FFT filtering."""
    n = len(x)
    responses = channel_responses(n, gamma, table, saturation, clip_boost, rate)
    spectrum = np.fft.rfft(x)
    return [np.fft.irfft(spectrum * responses[c], n) for c in range(3)]


def smooth(x, dt, attack, release):
    """One-pole follower: fast towards rising input, slow towards falling."""
    a_up = 1 - np.exp(-dt / attack) if attack > 0 else 1.0
    a_down = 1 - np.exp(-dt / release) if release > 0 else 1.0
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc += (v - acc) * (a_up if v > acc else a_down)
        y[i] = acc
    return y


def maxbin_levels(mono, rate, block, fft_size, table, gamma, saturation, clip_boost):
    """Per-block channel level = the strongest single STFT bin weighted by the
    channel's response, so tonal content counts fully and broadband content
    (spread over many bins) counts less. Scaled so a sine reads its amplitude."""
    weights = channel_responses(fft_size, gamma, table, saturation, clip_boost, rate)
    window = np.hanning(fft_size)
    scale = 2 / window.sum()
    n_blocks = len(mono) // block
    padded = np.pad(mono, (fft_size // 2, fft_size))
    out = np.empty((n_blocks, 3))
    chunk = 2048
    for start in range(0, n_blocks, chunk):
        idx = np.arange(start, min(start + chunk, n_blocks)) * block + block // 2
        frames = padded[idx[:, None] + np.arange(fft_size)] * window
        mag = np.abs(np.fft.rfft(frames, axis=1)) * scale
        out[start:start + len(idx)] = (mag[:, None, :] * weights[None]).max(2)
    return out


def colour_block(rate):
    """Colour track resolution: 1 ms in samples."""
    return int(round(rate / 1000))


def colour_track(mono, rate, args, filt=None):
    """Per-block (1 ms) smoothed band levels, float (n_blocks, 3). `mono` is
    float in [-1, 1)."""
    filt = filt or args.filter
    block = colour_block(rate)
    n_blocks = len(mono) // block
    dt = block / rate
    releases = (args.release_low, args.release_mid, args.release_high)
    design = dict(gamma=args.gamma, table=args.table, saturation=args.saturation,
                  clip_boost=args.clip_boost)
    if args.envelope == "maxbin":
        frames = maxbin_levels(mono, rate, block, args.fft_size, args.table,
                               args.gamma, args.saturation, args.clip_boost)
    else:
        filtered = (fir_filter_channels if filt == "fir" else fft_filter_channels)(mono, rate, **design)
    out = []
    for c in range(3):
        if args.envelope == "maxbin":
            peak = frames[:, c][:n_blocks]
        elif args.envelope == "rms":
            # Centred RMS over --rms-window, scaled so a sine reads its peak.
            power = (filtered[c][:n_blocks * block] ** 2).reshape(n_blocks, block).mean(1)
            w = max(1, int(args.rms_window / dt))
            power = np.convolve(power, np.ones(w) / w, mode="same")
            peak = np.sqrt(2 * power)
        else:
            peak = np.abs(filtered[c][:n_blocks * block]).reshape(n_blocks, block).max(1)
        level = peak ** args.gamma
        s = smooth(level, dt, args.attack, releases[c])
        if c == 0 and args.lookahead_low > 0:
            # Section 8.3: the low band's colour appears before a kick's onset.
            s = np.maximum(s, smooth(level[::-1], dt, args.attack, args.lookahead_low)[::-1])
        out.append(s)
    return np.stack(out, 1)


def to_u16(levels):
    """The stored form of the colour track: round(level * 65535), clipped."""
    return np.clip(np.floor(levels * 65535 + 0.5), 0, 65535).astype(np.uint16)


def deck_colour(levels, saturation):
    """Raise the unit colour to `saturation` (1 = unchanged), keeping the
    colour's length."""
    norm = np.linalg.norm(levels, axis=-1, keepdims=True)
    hue = (levels / np.maximum(norm, 1e-12)) ** saturation
    hue /= np.maximum(np.linalg.norm(hue, axis=-1, keepdims=True), 1e-12)
    return hue * norm


def shade_colour(levels, args):
    """Band levels -> RGB (before density), as the renderer's shader does."""
    colours = deck_colour(levels, args.saturation)
    if args.brightness_gamma:
        # Hue comes from levels**gamma; brightness follows level**brightness_gamma.
        norm = np.linalg.norm(colours, axis=-1, keepdims=True)
        colours = colours * norm ** (args.brightness_gamma / args.gamma - 1)
    return colours * np.array(args.channel_gains)


# ---------------------------------------------------------------- lane model


@dataclass
class View:
    position: float          # frame under the play marker
    samples_per_px: float
    width: int
    height: int
    playhead_x: float
    window_cols: float = 2.0


@dataclass
class Lane:
    status: np.ndarray       # per column: OUTSIDE / SIGNAL / SILENCE
    row_min: np.ndarray
    row_max: np.ndarray
    density: np.ndarray      # (height, width)
    levels: np.ndarray       # (width, 3) in 0..1, interpolated between 1 ms blocks


def lane_columns(times, values, view, colour_u16=None, block=44):
    """The Phosphor lane model for one view, from a detail tier's points."""
    h, w = view.height, view.width
    centre_row = (h - 1) / 2
    half = h / 2 * 0.8
    span = view.window_cols * view.samples_per_px / 2
    cs = view.position + (np.arange(w) + 0.5 - view.playhead_x) * view.samples_per_px
    n = len(times)
    status = np.zeros(w, np.uint8)
    row_min = np.zeros(w, np.int32)
    row_max = np.zeros(w, np.int32)
    density = np.zeros((h, w))
    for x, c in enumerate(cs):
        lo = np.searchsorted(times, c - span, "left")
        hi = np.searchsorted(times, c + span, "right") - 1
        i0 = np.searchsorted(times, c, "right") - 1
        lo, hi = max(min(lo, i0), 0), min(max(hi, i0 + 1), n - 1)
        if hi - lo < 1:
            continue
        v = values[lo:hi + 1]
        t = times[lo:hi + 1]
        rows = np.clip(np.floor(centre_row + v / 32768.0 * half + 0.5).astype(int), 0, h - 1)
        lo_r = np.minimum(rows[:-1], rows[1:])
        hi_r = np.maximum(rows[:-1], rows[1:])
        weight = np.diff(t) / (hi_r - lo_r + 1)
        acc = np.zeros(h + 1)
        np.add.at(acc, lo_r, weight)
        np.add.at(acc, hi_r + 1, -weight)
        density[:, x] = np.cumsum(acc)[:h] / (t[-1] - t[0]) * h / 2
        row_min[x], row_max[x] = rows.min(), rows.max()
        status[x] = SIGNAL if v.any() else SILENCE
    levels = np.zeros((w, 3))
    if colour_u16 is not None and len(colour_u16):
        u = np.clip(cs / block - 0.5, 0, len(colour_u16) - 1)
        k0 = np.floor(u).astype(int)
        k1 = np.minimum(k0 + 1, len(colour_u16) - 1)
        frac = (u - k0)[:, None]
        c16 = colour_u16.astype(np.float64) / 65535
        levels = c16[k0] * (1 - frac) + c16[k1] * frac
        levels[status == OUTSIDE] = 0
    return Lane(status, row_min, row_max, density, levels)


def shade(lane, args):
    """Lane model -> image: add (or opaque) light on RGB 24, black centre row in
    digital silence, no axis line."""
    h, w = lane.density.shape
    image = np.empty((h, w, 3))
    image[:] = np.array(BACKGROUND) / 255
    colours = shade_colour(lane.levels, args)
    for x in range(w):
        if lane.status[x] == OUTSIDE:
            continue
        covered = lane.density[:, x] > 1e-9
        pixels = colours[x][None, :] * args.gain * lane.density[:, x, None]
        if args.blend == "add":
            pixels = pixels + image[:, x]
        image[covered, x] = np.clip(pixels[covered], 0, 1)
        if lane.status[x] == SILENCE:
            image[h // 2, x] = 0
    return (image * 255 + 0.5).astype(np.uint8)


class Track:
    """A decoded track with its Phosphor data."""

    def __init__(self, audio, rate, args, filt=None):
        self.rate = rate
        self.mono = mono_int16(audio[:, 0], audio[:, 1])
        self.levels = colour_track(self.mono / 32768.0, rate, args, filt)
        self.colour = to_u16(self.levels)
        self._tiers = {}

    def tier(self, n):
        if n not in self._tiers:
            self._tiers[n] = build_tier(self.mono, n)
        return self._tiers[n]

    def lane(self, t_play, pps, args, tier=1):
        view = View(position=t_play * self.rate, samples_per_px=self.rate / pps,
                    width=args.width, height=args.height, playhead_x=args.playhead_x,
                    window_cols=args.shape_window)
        times, values = self.tier(tier)
        return lane_columns(times, values, view, self.colour, colour_block(self.rate))

    def render(self, t_play, pps, args, tier=1):
        return shade(self.lane(t_play, pps, args, tier), args)


# ---------------------------------------------------------------- fixtures

FIXTURE_VERSION = 1
FIXTURE_TIERS = (4, 8, 16, 32)


def _section(tag, payload):
    return tag.encode("ascii") + struct.pack("<I", len(payload)) + payload


def lane_section(name, tier_n, tier, rate, colour, spec):
    """One LANE section. spec: dict(position, px_per_bar, bpm, width, height,
    playhead_x, window_cols)."""
    pps = px_per_sec(spec["px_per_bar"], spec["bpm"])
    view = View(position=spec["position"], samples_per_px=rate / pps,
                width=spec["width"], height=spec["height"],
                playhead_x=spec["playhead_x"], window_cols=spec["window_cols"])
    lane = lane_columns(tier[0], tier[1], view, colour, colour_block(rate))
    label_bytes = name.encode("utf-8")
    p = struct.pack("<H", len(label_bytes)) + label_bytes
    p += struct.pack("<IddddIII", tier_n, spec["position"], spec["px_per_bar"],
                     spec["bpm"], spec["playhead_x"], view.width, view.height,
                     int(round(spec["window_cols"] * 1000)))
    p += lane.status.astype("<u1").tobytes()
    p += lane.row_min.astype("<i4").tobytes()
    p += lane.row_max.astype("<i4").tobytes()
    p += lane.levels.astype("<f4").tobytes()
    p += np.ascontiguousarray(lane.density.T).astype("<f4").tobytes()
    return _section("LANE", p)


def build_fixture(name, rate, left, right, lane_cases, args=None):
    """Bytes of one .phfx file for the stereo int16 excerpt (left, right).
    lane_cases: list of (case_name, tier_n, spec)."""
    args = args or default_args()
    left, right = np.asarray(left, np.int16), np.asarray(right, np.int16)
    mono = mono_int16(left, right)
    is_mono = bool((left == right).all())
    inpt = struct.pack("<III", rate, 1 if is_mono else 2, len(left))
    inpt += (left.astype("<i2").tobytes() if is_mono
             else np.stack([left, right], 1).astype("<i2").tobytes())
    out = b"PHFX" + struct.pack("<I", FIXTURE_VERSION) + _section("INPT", inpt)
    tiers = {1: build_tier(mono, 1)}
    for n in FIXTURE_TIERS:
        tiers[n] = build_tier(mono, n)
        out += _section("TIER", struct.pack("<II", n, len(tiers[n][1]))
                        + tiers[n][1].astype("<i2").tobytes())
    colour = to_u16(colour_track(mono / 32768.0, rate, args, "fir"))
    out += _section("COLR", struct.pack("<II", colour_block(rate), len(colour))
                    + colour.astype("<u2").tobytes())
    for case_name, tier_n, spec in lane_cases:
        if tier_n not in tiers:
            tiers[tier_n] = build_tier(mono, tier_n)
        out += lane_section(case_name, tier_n, tiers[tier_n], rate, colour, spec)
    return out


def read_fixture(data):
    """Parse a .phfx file into [(tag, dict)] (the format reader, for tests)."""
    assert data[:4] == b"PHFX"
    assert struct.unpack_from("<I", data, 4)[0] == FIXTURE_VERSION
    pos, out = 8, []
    while pos < len(data):
        tag = data[pos:pos + 4].decode("ascii")
        (size,) = struct.unpack_from("<I", data, pos + 4)
        p = data[pos + 8:pos + 8 + size]
        pos += 8 + size
        if tag == "INPT":
            rate, ch, frames = struct.unpack_from("<III", p)
            samples = np.frombuffer(p, "<i2", offset=12).reshape(frames, ch)
            out.append((tag, dict(rate=rate, channels=ch, samples=samples)))
        elif tag == "TIER":
            n, count = struct.unpack_from("<II", p)
            out.append((tag, dict(n=n, values=np.frombuffer(p, "<i2", count, 8))))
        elif tag == "COLR":
            block, count = struct.unpack_from("<II", p)
            out.append((tag, dict(block=block,
                                  levels=np.frombuffer(p, "<u2", count * 3, 8).reshape(count, 3))))
        elif tag == "LANE":
            (ln,) = struct.unpack_from("<H", p)
            name = p[2:2 + ln].decode("utf-8")
            o = 2 + ln
            tier_n, position, ppb, bpm, phx, w, h, win = struct.unpack_from("<IddddIII", p, o)
            o += struct.calcsize("<IddddIII")
            status = np.frombuffer(p, "<u1", w, o)
            o += w
            row_min = np.frombuffer(p, "<i4", w, o)
            o += 4 * w
            row_max = np.frombuffer(p, "<i4", w, o)
            o += 4 * w
            levels = np.frombuffer(p, "<f4", w * 3, o).reshape(w, 3)
            o += 12 * w
            density = np.frombuffer(p, "<f4", w * h, o).reshape(w, h).T
            out.append((tag, dict(name=name, tier=tier_n, position=position, px_per_bar=ppb,
                                  bpm=bpm, playhead_x=phx, width=w, height=h,
                                  window_cols=win / 1000, status=status, row_min=row_min,
                                  row_max=row_max, levels=levels, density=density)))
        else:
            raise ValueError(tag)
    return out


# Excerpts of the test-signal WAVs: (fixture name, wav file, start s).
EXCERPT_SECONDS = 0.2
EXCERPTS = [
    ("q2a_630hz", "Q2a crossover steps.wav", 30.95),
    ("q2b_sweep", "Q2b sweep 20Hz-20kHz.wav", 31.0),
    ("q3a_100hz_step", "Q3a amplitude steps 100Hz.wav", 3.95),
    ("q3b_1khz_step", "Q3b amplitude steps 1kHz.wav", 3.95),
    ("q3c_8khz_step", "Q3c amplitude steps 8kHz.wav", 3.95),
    ("q4_sine", "Q4 sine vs square.wav", 2.0),
    ("q4_square", "Q4 sine vs square.wav", 7.0),
    ("q4_silence", "Q4 sine vs square.wav", 0.0),
    ("q5_click", "Q5 clicks and kicks.wav", 1.95),
    ("q5_kick", "Q5 clicks and kicks.wav", 19.95),
    ("q6_sine", "Q6 sine vs noise.wav", 7.0),
    ("q6_noise", "Q6 sine vs noise.wav", 11.5),
    ("q7_inverted", "Q7 stereo L R both inverted.wav", 17.0),
    ("q9_mixture", "Q9 two-tone mixtures.wav", 11.5),
]

Z1 = px_per_bar_for_zoom(1)  # 869.6 px per bar
DEEP = 88200.0  # px per bar at 120 BPM = one audio frame per column at 44.1 kHz


def _view(position, ppb, bpm=120.0, width=40, height=41, window_cols=2.0):
    return dict(position=position, px_per_bar=ppb, bpm=bpm, width=width, height=height,
                playhead_x=width / 2, window_cols=window_cols)


# Lane cases per excerpt: (case name, tier N, view). Positions are frames.
LANE_CASES = {
    "q4_square": [("hollow_square", 1, _view(4410, Z1)),
                  ("hollow_square_tier8", 8, _view(4410, Z1))],
    "q4_sine": [("sine_edges", 1, _view(4410, Z1)),
                ("sine_no_bpm", 1, _view(4410, Z1, bpm=0.0)),
                ("sine_deep_tier8", 8, _view(4410, DEEP))],
    "q6_noise": [("noise_fill", 1, _view(4410, Z1)),
                 ("noise_tier32", 32, _view(4410, Z1))],
    "q2a_630hz": [("tone_630hz", 1, _view(4410, 2 * Z1))],
    "q5_click": [("click_z4", 1, _view(2205, px_per_bar_for_zoom(4) * 4)),
                 ("click_deep", 1, _view(2205, DEEP))],
    "q5_kick": [("kick_tier4", 4, _view(3300, Z1))],
    "q4_silence": [("digital_silence", 1, _view(4410, Z1))],
    "q7_inverted": [("left_minus_right", 1, _view(4410, Z1))],
}


def read_wav(path):
    with wave.open(path, "rb") as f:
        assert f.getsampwidth() == 2 and f.getnchannels() == 2
        rate = f.getframerate()
        data = np.frombuffer(f.readframes(f.getnframes()), "<i2").reshape(-1, 2)
    return rate, data[:, 0], data[:, 1]


def write_fixtures(out_dir, signals_dir):
    os.makedirs(out_dir, exist_ok=True)
    here = os.path.dirname(os.path.abspath(__file__))
    if not os.path.isdir(signals_dir) or not any(f.endswith(".wav") for f in os.listdir(signals_dir)):
        subprocess.run([sys.executable, os.path.join(here, "waveform_test_signals.py"), signals_dir],
                       check=True, stdout=subprocess.DEVNULL)
    cache = {}
    for name, wav, start in EXCERPTS:
        if wav not in cache:
            cache[wav] = read_wav(os.path.join(signals_dir, wav))
        rate, left, right = cache[wav]
        a = int(round(start * rate))
        b = a + int(round(EXCERPT_SECONDS * rate))
        data = build_fixture(name, rate, left[a:b], right[a:b], LANE_CASES.get(name, []))
        with open(os.path.join(out_dir, name + ".phfx"), "wb") as f:
            f.write(data)
        print(f"wrote {name}.phfx ({len(data)} bytes)")


# ---------------------------------------------------------------- checks


def check_fir(args):
    """FFT vs FIR colours on the five reference tracks. The difference is
    measured on the colour a pixel of density 1 would get (0-255 per channel):
    unclipped, so the clip cannot hide a difference."""
    print("FFT vs FIR colours, per-channel difference in levels (0-255), "
          "colour x density-1 brightness, unclipped")
    print(f"{'track':18} {'max R':>7} {'max G':>7} {'max B':>7} {'mean R':>7} {'mean G':>7} {'mean B':>7}")
    worst = 0.0
    for key, filename, _, _ in TRACKS:
        audio = decode(os.path.join(args.tracks_dir, filename))
        mono = mono_int16(audio[:, 0], audio[:, 1]) / 32768.0
        a = shade_colour(colour_track(mono, RATE, args, "fft"), args) * args.gain * 255
        b = shade_colour(colour_track(mono, RATE, args, "fir"), args) * args.gain * 255
        d = np.abs(a - b)
        worst = max(worst, d.max())
        print(f"{key:18} " + " ".join(f"{v:7.3f}" for v in [*d.max(0), *d.mean(0)]), flush=True)
    print(f"worst per-channel max difference: {worst:.3f} levels "
          f"({'PASS' if worst < 2 else 'FAIL'} against the < 2 criterion)")


def label(img, text):
    bar = Image.new("RGB", (img.width, 18), (0, 0, 0))
    ImageDraw.Draw(bar).text((6, 3), text, fill=(220, 220, 220))
    out = Image.new("RGB", (img.width, img.height + 18))
    out.paste(bar, (0, 0))
    out.paste(img, (0, 18))
    return out


def zoom(img, x0, x1, factor):
    crop = img.crop((x0, 0, x1, img.height))
    return crop.resize((crop.width * factor, crop.height * factor), Image.NEAREST)


def stack(images, gap=6):
    w = max(i.width for i in images)
    h = sum(i.height for i in images) + gap * (len(images) - 1)
    out = Image.new("RGB", (w, h), (60, 60, 60))
    y = 0
    for i in images:
        out.paste(i, (0, y))
        y += i.height + gap
    return out


def check_tiers(args):
    """Detail tiers vs Full: mean and max pixel difference (0-255 per channel)
    on the five tracks, at the fitted play marker plus four more positions,
    and a comparison image per track at the fitted position."""
    os.makedirs(args.tier_images, exist_ok=True)
    names = [n for n in TIERS if n != "full"]
    means = {n: [] for n in names}
    maxes = {n: [] for n in names}
    print("Detail tiers vs Full, pixel difference per channel (0-255); 5 positions per track "
          "(fitted play marker, then 20/40/60/80 % of the track)")
    for key, filename, t_fit, pps in TRACKS:
        track = Track(decode(os.path.join(args.tracks_dir, filename)), RATE, args)
        dur = len(track.mono) / RATE
        positions = [t_fit] + [dur * f for f in (0.2, 0.4, 0.6, 0.8)]
        per = {n: ([], []) for n in names}
        for pos in positions:
            full = shade(track.lane(pos, pps, args, 1), args).astype(int)
            for n in names:
                d = np.abs(shade(track.lane(pos, pps, args, TIERS[n]), args).astype(int) - full)
                per[n][0].append(d.mean())
                per[n][1].append(d.max())
        px = int(args.playhead_x)
        full_img = Image.fromarray(track.render(t_fit, pps, args, 1))
        sheet = [label(zoom(full_img, px - 150, px + 150, 4), f"{key}: Full, 4x around the play marker")]
        for n in names:
            img = Image.fromarray(track.render(t_fit, pps, args, TIERS[n]))
            d = np.abs(np.asarray(img).astype(int) - np.asarray(full_img).astype(int))
            sheet.append(label(zoom(img, px - 150, px + 150, 4),
                               f"{n} (N={TIERS[n]}): mean diff {d.mean():.3f}, max {d.max()} at this position"))
        stack(sheet).save(os.path.join(args.tier_images, f"{key}.png"), optimize=True)
        for n in names:
            means[n].append(float(np.mean(per[n][0])))
            maxes[n].append(float(np.max(per[n][1])))
            print(f"{key:18} {n:7} N={TIERS[n]:<3} mean {means[n][-1]:.4f}  max {maxes[n][-1]:.0f}", flush=True)
    print("\nSummary over the five tracks:")
    for n in names:
        print(f"{n:7} N={TIERS[n]:<3} mean {np.mean(means[n]):.4f}  max {np.max(maxes[n]):.0f}")


# ---------------------------------------------------------------- cli

# Section 8 numbers as measured on the stripe, with no screenshot tuning.
LITERAL = dict(brightness_gamma=0.0, table="stripe", shape_window=1.0, saturation=1.0,
               channel_gains=[1.0, 1.0, 1.0], gain=0.7, gamma=0.25, attack=0.02,
               release_high=0.09)


def build_parser():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("audio", nargs="?")
    p.add_argument("--time", type=float, default=176 * 1.6,
                   help="track time (s) under the playhead; default: the bar at 04:41 of Gigando")
    p.add_argument("--px-per-bar", type=float, default=px_per_bar_for_zoom(4),
                   help="zoom in px per bar (= 869.6 / z; a bar is 4 beats)")
    p.add_argument("--zoom", type=float, help="deck zoom value z; overrides --px-per-bar")
    p.add_argument("--bpm", type=float, default=0.0, help="track BPM (0 = none, treated as 120)")
    p.add_argument("--px-per-sec", type=float,
                   help="explicit px/s, overrides the per-bar zoom (the fitted values)")
    p.add_argument("--width", type=int, default=SHOT_LANE[2] - SHOT_LANE[0])
    p.add_argument("--height", type=int, default=SHOT_LANE[3] - SHOT_LANE[1])
    p.add_argument("--playhead-x", type=float, default=SHOT_PLAYHEAD_X - SHOT_LANE[0])
    p.add_argument("--shape-window", type=float, default=2.0,
                   help="columns of audio each column's outline covers (1-6)")
    p.add_argument("--detail", choices=list(TIERS), default="high",
                   help="Waveform detail tier: block of 1/4/8/16/32 samples")
    p.add_argument("--filter", choices=["fir", "fft"], default="fir",
                   help="colour filters: linear-phase FIR (spec) or whole-track FFT (prototype)")
    p.add_argument("--table", choices=["deck", "stripe"], default="stripe",
                   help="stripe + saturation looks closest on music; deck matches pure tones")
    p.add_argument("--envelope", choices=["peak", "rms", "maxbin"], default="peak",
                   help="colour band level: peak per 1 ms, centred RMS, or strongest STFT bin")
    p.add_argument("--fft-size", type=int, default=2048)
    p.add_argument("--clip-boost", type=float, default=1.0,
                   help="multiplier for deck-table entries that were clipped at 255")
    p.add_argument("--rms-window", type=float, default=0.02)
    p.add_argument("--saturation", type=float, default=5.0)
    p.add_argument("--channel-gains", type=lambda v: [float(c) for c in v.split(",")],
                   default=[1.0, 0.85, 1.8], help="R,G,B multipliers on the deck colour")
    p.add_argument("--blend", choices=["add", "opaque"], default="add",
                   help="add density light to the background, or replace it")
    p.add_argument("--gain", type=float, default=0.7, help="density brightness k")
    p.add_argument("--gamma", type=float, default=0.35, help="colour level compression")
    p.add_argument("--brightness-gamma", type=float, default=0.87,
                   help="separate level exponent for brightness (Q3 gives ~0.87); 0 = use --gamma")
    p.add_argument("--attack", type=float, default=0.005)
    p.add_argument("--release-low", type=float, default=0.35)
    p.add_argument("--release-mid", type=float, default=0.18)
    p.add_argument("--release-high", type=float, default=0.04)
    p.add_argument("--lookahead-low", type=float, default=0.12,
                   help="backward release for the low band (0 = causal)")
    p.add_argument("--compare", help="Traktor screenshot to put alongside")
    p.add_argument("--out", default="phosphor_render")
    p.add_argument("--check-fir", action="store_true", help="FFT vs FIR colours on the five tracks")
    p.add_argument("--check-tiers", action="store_true", help="detail tiers vs Full on the five tracks")
    p.add_argument("--tracks-dir", default=TRACKS_DIR)
    p.add_argument("--tier-images", default="docs/research/phosphor-detail-tiers")
    p.add_argument("--fixtures", metavar="DIR", help="write the C++ test fixtures to DIR")
    p.add_argument("--signals-dir", default="waveform-tests",
                   help="Q2-Q9 test-signal WAVs (generated by waveform_test_signals.py if missing)")
    return p


def default_args():
    return build_parser().parse_args([])


def main():
    parser = build_parser()
    args = parser.parse_args()
    if args.fixtures:
        write_fixtures(args.fixtures, args.signals_dir)
        return
    if args.check_fir:
        check_fir(args)
        return
    if args.check_tiers:
        check_tiers(args)
        return
    if not args.audio:
        parser.error("audio file required")

    if args.zoom:
        args.px_per_bar = px_per_bar_for_zoom(args.zoom)
    pps = args.px_per_sec or px_per_sec(args.px_per_bar, args.bpm)
    tier = TIERS[args.detail]
    track = Track(decode(args.audio), RATE, args)
    ours = Image.fromarray(track.render(args.time, pps, args, tier))
    ours.save(args.out + "_render.png")
    print("wrote", args.out + "_render.png")
    if not args.compare:
        return

    literal_args = argparse.Namespace(**{**vars(args), **LITERAL})
    literal_track = Track(decode(args.audio), RATE, literal_args)
    literal = Image.fromarray(literal_track.render(args.time, pps, literal_args, tier))
    shot = Image.open(args.compare).convert("RGB").crop(SHOT_LANE)

    def describe(a):
        gains = ",".join(f"{g:g}" for g in a.channel_gains)
        return (f"min..max over {a.shape_window:g} columns, detail {a.detail}, density x{a.gain:g}, "
                f"gamma {a.gamma:g} (brightness {a.brightness_gamma or a.gamma:g}), "
                f"saturation {a.saturation:g}, RGB x{gains}, "
                f"attack {a.attack * 1000:g} ms, release {a.release_low * 1000:g}/"
                f"{a.release_mid * 1000:g}/{a.release_high * 1000:g} ms")

    px = int(args.playhead_x)
    z0, z1 = px - 235, px + 235
    sheet = stack([
        label(shot, f"Traktor Pro 4, deck B (playhead = red line at {args.time:.2f} s), Spectrum, native pixels"),
        label(ours, "Phosphor reference, tuned: " + describe(args)),
        label(literal, "Phosphor reference, section 8 literal: " + describe(literal_args)),
        label(zoom(shot, z0, z1, 4), "Traktor, 4x around the playhead"),
        label(zoom(ours, z0, z1, 4), "Phosphor tuned, 4x"),
        label(zoom(literal, z0, z1, 4), "Phosphor section 8 literal, 4x"),
    ])
    sheet.save(args.out + "_comparison.png")
    print("wrote", args.out + "_comparison.png")


if __name__ == "__main__":
    main()
