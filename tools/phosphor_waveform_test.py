# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow"]
# ///
"""Behaviour tests for the Phosphor reference script (tools/phosphor_waveform.py).

Run: uv run tools/phosphor_waveform_test.py
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import phosphor_waveform as pw  # noqa: E402


class Zoom(unittest.TestCase):
    def test_per_bar_zoom_matches_spec(self):
        self.assertAlmostEqual(pw.px_per_bar_for_zoom(4), 217.4)
        # 150 BPM: a bar is 1.6 s, so 217.4 px per bar is 135.875 px/s.
        self.assertAlmostEqual(pw.px_per_sec(217.4, 150), 135.875)

    def test_track_without_bpm_behaves_as_120(self):
        self.assertEqual(pw.px_per_sec(217.4, 0), pw.px_per_sec(217.4, 120))


class Tiers(unittest.TestCase):
    def test_block_keeps_min_and_max_in_order_of_occurrence(self):
        mono = np.array([1, 5, 3, 2, 9, -4, 0, 0], np.int16)
        times, values = pw.build_tier(mono, 4)
        self.assertEqual(values.tolist(), [1, 5, 9, -4])
        self.assertEqual(times.tolist(), [1.0, 3.0, 5.0, 7.0])

    def test_full_tier_is_every_sample(self):
        mono = np.array([3, -3, 7], np.int16)
        times, values = pw.build_tier(mono, 1)
        self.assertEqual(values.tolist(), [3, -3, 7])
        self.assertEqual(times.tolist(), [0.0, 1.0, 2.0])

    def test_partial_last_block_is_kept(self):
        mono = np.array([0, 1, 2, 3, 4, -4], np.int16)
        times, values = pw.build_tier(mono, 4)
        self.assertEqual(values.tolist(), [0, 3, 4, -4])
        self.assertEqual(len(values), 4)


class LaneModel(unittest.TestCase):
    H = 41

    def lane(self, mono, spp, pos, tier=1, width=5, window=2.0):
        t, v = pw.build_tier(np.asarray(mono, np.int16), tier)
        view = pw.View(position=pos, samples_per_px=spp, width=width,
                       height=self.H, playhead_x=width / 2, window_cols=window)
        return pw.lane_columns(t, v, view)

    def test_deep_zoom_window_never_drops_below_the_two_samples_around_centre(self):
        mono = [0, 16384, -16384, 0, 0, 0, 0, 0]
        lane = self.lane(mono, 0.1, 1.5, width=1)
        c = (self.H - 1) / 2
        half = self.H / 2 * 0.8
        lo = int(np.floor(c - 0.5 * half + 0.5))
        hi = int(np.floor(c + 0.5 * half + 0.5))
        # Samples 1 (+0.5) and 2 (-0.5) surround the centre at 1.5 + 0.05.
        self.assertEqual((lane.row_min[0], lane.row_max[0]), (lo, hi))
        self.assertGreater(lane.density[lo:hi + 1, 0].sum(), 0)

    def test_positive_samples_are_drawn_below_the_centre(self):
        lane = self.lane([20000] * 200, 4.0, 100, width=3)
        self.assertGreater(lane.row_min[1], (self.H - 1) / 2)

    def test_digital_silence_is_flagged(self):
        lane = self.lane([0] * 200, 4.0, 100, width=3)
        self.assertEqual(lane.status.tolist(), [pw.SILENCE] * 3)

    def test_columns_outside_the_track_have_no_signal(self):
        lane = self.lane([1000] * 20, 4.0, 5, width=9)
        self.assertEqual(lane.status[0], pw.OUTSIDE)
        self.assertEqual(lane.status[4], pw.SIGNAL)

    def test_decimated_tier_keeps_the_outline(self):
        rng = np.random.default_rng(1)
        mono = (rng.uniform(-1, 1, 4000) * 20000).astype(np.int16)
        a = self.lane(mono, 40.0, 2000, tier=1, width=20, window=2.0)
        b = self.lane(mono, 40.0, 2000, tier=8, width=20, window=2.0)
        # Window edges fall inside blocks, so allow a row of slack at the ends.
        self.assertLessEqual(np.abs(a.row_min - b.row_min).max(), 2)
        self.assertLessEqual(np.abs(a.row_max - b.row_max).max(), 2)

    def test_density_of_a_sine_is_bright_at_the_edges(self):
        n = np.arange(8000)
        mono = (np.sin(2 * np.pi * n / 400) * 20000).astype(np.int16)
        lane = self.lane(mono, 100.0, 4000, width=1, window=6.0)
        col = lane.density[:, 0]
        c = self.H // 2
        self.assertGreater(col[lane.row_min[0]], col[c])


class Fir(unittest.TestCase):
    RATE = 44100

    def test_impulse_response_is_delay_compensated(self):
        x = np.zeros(20000)
        x[7000] = 1.0
        for y in pw.fir_filter_channels(x, self.RATE):
            self.assertEqual(int(np.argmax(np.abs(y))), 7000)

    def test_matches_whole_track_fft_on_tones(self):
        n = np.arange(self.RATE * 2)
        x = 0.5 * np.sin(2 * np.pi * 1000 * n / self.RATE) + 0.3 * np.sin(2 * np.pi * 60 * n / self.RATE)
        fir = pw.fir_filter_channels(x, self.RATE)
        fft = pw.fft_filter_channels(x, self.RATE)
        mid = slice(self.RATE // 2, -self.RATE // 2)
        for a, b in zip(fir, fft):
            self.assertLess(np.abs(a[mid] - b[mid]).max(), 0.02)

    def test_taps_scale_with_sample_rate(self):
        self.assertEqual(pw.fir_length(44100), 4096)
        self.assertEqual(pw.fir_length(48000), 4458)


class Fixtures(unittest.TestCase):
    def test_generation_is_byte_identical_and_parses(self):
        rng = np.random.default_rng(2)
        left = (rng.uniform(-1, 1, 4410) * 9000).astype(np.int16)
        a = pw.build_fixture("t", 44100, left, -left, [])
        b = pw.build_fixture("t", 44100, left, -left, [])
        self.assertEqual(a, b)
        sections = pw.read_fixture(a)
        self.assertEqual([s[0] for s in sections][:1], ["INPT"])
        tiers = [s for s in sections if s[0] == "TIER"]
        self.assertEqual([t[1]["n"] for t in tiers], [4, 8, 16, 32])
        # L = -R sums to digital silence.
        self.assertTrue(all((t[1]["values"] == 0).all() for t in tiers))
        colr = [s for s in sections if s[0] == "COLR"][0][1]
        self.assertEqual(colr["levels"].shape, (4410 // 44, 3))


if __name__ == "__main__":
    unittest.main()
