#!/usr/bin/env python3
"""Generate test signals for black-box comparison of DJ software waveforms.

Each file targets one open question from docs/research/traktor-waveform.md
(Q2-Q7). Load the files in Traktor (and Mixxx), screenshot the deck
waveform at a fixed zoom, and compare against the known signal content.

Segments are separated by one second of silence so their boundaries are
easy to find on screen. All files are 44.1 kHz, 16-bit, stereo.

Usage: python tools/waveform_test_signals.py [output_dir]
"""

import array
import math
import os
import random
import sys
import wave

RATE = 44100
FADE_SECONDS = 0.005


def db(level_dbfs):
    return 10.0 ** (level_dbfs / 20.0)


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def faded(samples):
    """Apply short linear fades so segment edges don't add clicks."""
    n = min(int(FADE_SECONDS * RATE), len(samples) // 2)
    for i in range(n):
        gain = i / n
        samples[i] *= gain
        samples[-1 - i] *= gain
    return samples


def sine(freq, seconds, peak):
    w = 2.0 * math.pi * freq / RATE
    return faded([peak * math.sin(w * i) for i in range(int(seconds * RATE))])


def square(freq, seconds, peak):
    period = RATE / freq
    return faded([peak if (i % period) < period / 2 else -peak
                  for i in range(int(seconds * RATE))])


def white_noise(seconds, rms, seed):
    # Uniform noise in [-a, a] has RMS a / sqrt(3).
    rng = random.Random(seed)
    a = rms * math.sqrt(3.0)
    return faded([rng.uniform(-a, a) for _ in range(int(seconds * RATE))])


def log_sweep(f_start, f_end, seconds, peak):
    n = int(seconds * RATE)
    k = math.log(f_end / f_start)
    phase_scale = 2.0 * math.pi * f_start * seconds / k
    return faded([peak * math.sin(phase_scale * (math.exp(k * i / n) - 1.0))
                  for i in range(n)])


def hann_burst(freq, seconds, peak):
    n = int(seconds * RATE)
    w = 2.0 * math.pi * freq / RATE
    return [peak * math.sin(w * i) * 0.5 * (1.0 - math.cos(2.0 * math.pi * i / (n - 1)))
            for i in range(n)]


def concat(*parts):
    out = []
    for part in parts:
        out.extend(part)
    return out


def with_gaps(segments, gap=1.0):
    """Join segments with silence before, between and after them."""
    out = silence(gap)
    for segment in segments:
        out.extend(segment)
        out.extend(silence(gap))
    return out


def write_wav(path, left, right=None):
    if right is None:
        right = left
    assert len(left) == len(right)
    data = array.array("h")
    for l, r in zip(left, right):
        data.append(int(max(-1.0, min(1.0, l)) * 32767))
        data.append(int(max(-1.0, min(1.0, r)) * 32767))
    if sys.byteorder == "big":
        data.byteswap()
    with wave.open(path, "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data.tobytes())
    print(f"wrote {path} ({len(left) / RATE:.1f} s)")


def q2_crossover_steps():
    # Roughly 2/3-octave steps across the audible range, 4 s each at -6 dBFS.
    freqs = [40, 63, 100, 160, 250, 400, 630, 1000, 1600,
             2500, 4000, 6300, 10000, 16000]
    return with_gaps([sine(f, 4.0, db(-6)) for f in freqs])


def q2_sweep():
    return with_gaps([log_sweep(20.0, 20000.0, 60.0, db(-6))])


def q3_amplitude_steps(freq):
    levels = [0, -6, -12, -18, -24, -30, -36, -48]
    return with_gaps([sine(freq, 3.0, db(level)) for level in levels])


def q4_sine_vs_square():
    # Equal peak level: equal height if peaks are drawn, square ~1.41x taller
    # if RMS is drawn. Repeated at two frequencies because the square's
    # harmonics also land in higher bands.
    return with_gaps([sine(100, 4.0, db(-6)), square(100, 4.0, db(-6)),
                      sine(1000, 4.0, db(-6)), square(1000, 4.0, db(-6))])


def q5_transients():
    # Eight single-sample clicks, then eight 60 Hz kick-like bursts,
    # two seconds apart (every 4 beats at 120 BPM).
    click = [0.0] * int(2.0 * RATE)
    click[0] = db(-1)
    kick = concat(hann_burst(60, 0.05, db(-3)), silence(2.0 - 0.05))
    return concat(silence(2.0), click * 8, silence(2.0), kick * 8)


def q6_sine_vs_noise():
    # Matched RMS of -15 dBFS (a -12 dBFS peak sine).
    rms = db(-12) / math.sqrt(2.0)
    return with_gaps([sine(100, 4.0, db(-12)), sine(1000, 4.0, db(-12)),
                      white_noise(4.0, rms, seed=1),
                      white_noise(4.0, rms * db(-12), seed=2)])


def q7_stereo():
    tone = sine(1000, 4.0, db(-6))
    quiet = silence(len(tone) / RATE)
    gap = silence(1.0)
    left = concat(gap, tone, gap, quiet, gap, tone, gap, tone, gap)
    right = concat(gap, quiet, gap, tone, gap, tone, gap,
                   [-s for s in tone], gap)
    return left, right


def mix(*parts):
    return [sum(samples) for samples in zip(*parts)]


def q9_two_tone_mixtures():
    # How the colours of two bands combine: each tone alone, then pairs at
    # equal level and with either tone 12 dB down. 4 s per segment.
    def tone(freq, level):
        return sine(freq, 4.0, db(level))
    return with_gaps([
        tone(100, -12), tone(4000, -12),
        mix(tone(100, -12), tone(4000, -12)),
        mix(tone(100, -12), tone(4000, -24)),
        mix(tone(100, -24), tone(4000, -12)),
        mix(tone(100, -12), tone(1000, -12)),
        mix(tone(1000, -12), tone(4000, -12)),
    ])


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "waveform-tests"
    os.makedirs(out_dir, exist_ok=True)

    def path(name):
        return os.path.join(out_dir, name)

    write_wav(path("Q2a crossover steps.wav"), q2_crossover_steps())
    write_wav(path("Q2b sweep 20Hz-20kHz.wav"), q2_sweep())
    write_wav(path("Q3a amplitude steps 100Hz.wav"), q3_amplitude_steps(100))
    write_wav(path("Q3b amplitude steps 1kHz.wav"), q3_amplitude_steps(1000))
    write_wav(path("Q3c amplitude steps 8kHz.wav"), q3_amplitude_steps(8000))
    write_wav(path("Q4 sine vs square.wav"), q4_sine_vs_square())
    write_wav(path("Q5 clicks and kicks.wav"), q5_transients())
    write_wav(path("Q6 sine vs noise.wav"), q6_sine_vs_noise())
    write_wav(path("Q7 stereo L R both inverted.wav"), *q7_stereo())
    write_wav(path("Q9 two-tone mixtures.wav"), q9_two_tone_mixtures())


if __name__ == "__main__":
    main()
