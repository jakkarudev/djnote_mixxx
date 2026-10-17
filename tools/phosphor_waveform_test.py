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


def stripe_bytes(blocks=7, pixels=None):
    """A synthetic Traktor stripe file (format: research doc 4.1)."""
    import struct
    head = bytes([6]) + b"PRTS" + struct.pack("<IfIII", 2, 3.5, 1024, 64, blocks)
    body = np.zeros((1024, 64, 4), np.uint8) if pixels is None else pixels
    return head + body.tobytes()


class StripeReader(unittest.TestCase):
    def test_reads_block_count_and_column_major_pixels(self):
        px = np.zeros((1024, 64, 4), np.uint8)
        px[3, 5] = (10, 20, 30, 40)
        s = pw.read_stripe(stripe_bytes(blocks=20439, pixels=px))
        self.assertEqual(s.blocks, 20439)
        self.assertEqual(s.pixels.shape, (1024, 64, 4))
        self.assertEqual(s.pixels[3, 5].tolist(), [10, 20, 30, 40])
        raw = stripe_bytes(pixels=px)
        self.assertEqual(list(raw[25 + 4 * (3 * 64 + 5):][:4]), [10, 20, 30, 40])

    def test_rejects_other_files(self):
        with self.assertRaises(ValueError):
            pw.read_stripe(b"x" + b"STRP" + bytes(262169))
        with self.assertRaises(ValueError):
            pw.read_stripe(stripe_bytes()[:-1])


class Overview(unittest.TestCase):
    def render(self, mono, levels=None, blocks=None):
        mono = np.asarray(mono, np.float64)
        n = blocks or -(-len(mono) // 1024)
        if levels is None:
            levels = np.tile([0.1, 0.3, 0.5], (len(mono) // 44 + 1, 1))
        return pw.render_overview(mono, 44100, levels, n, pw.default_args())

    def square(self, amp, n=2048):
        x = np.full(n, amp)
        x[1::2] = -amp
        return x

    def rows(self, img, col):
        return np.nonzero(img[col].any(1))[0]

    def test_column_holds_n_samples_n_is_1024_sample_blocks_in_track(self):
        # A column holds N = `blocks` samples: 3 x 1024 samples fill 3 columns.
        img = self.render(self.square(0.5, 3 * 1024), blocks=1024)
        self.assertEqual(img.shape, (1024, 64, 4))
        for c in range(3):
            self.assertGreater(len(self.rows(img, c)), 0)
        self.assertEqual(len(self.rows(img, 1000)), 0)

    def test_height_is_linear_in_peak_about_row_30_5(self):
        # Research 8.1: 62 / 32 / 16 / 8 rows at 0 / -6 / -12 / -18 dBFS.
        got = [len(self.rows(self.render(self.square(a, 1024), blocks=1024), 0))
               for a in (1.0, 0.5, 0.25, 0.125)]
        self.assertEqual(got, [62, 32, 16, 8])
        r = self.rows(self.render(self.square(0.5, 1024), blocks=1024), 0)
        self.assertEqual((r[0], r[-1]), (15, 46))

    def test_positive_samples_are_drawn_downward(self):
        x = np.zeros(1024)
        x[100] = 0.9
        r = self.rows(self.render(x, blocks=1024), 0)
        self.assertGreaterEqual(r[0], 30)
        self.assertGreater(r[-1], 50)

    def test_colour_is_constant_in_column_and_alpha_follows_green(self):
        img = self.render(self.square(0.5, 1024), levels=np.tile([0.1, 0.4, 0.2], (100, 1)), blocks=1024)
        px = img[0][self.rows(img, 0)].astype(float)
        unit = px[:, :3] / np.linalg.norm(px[:, :3], axis=1, keepdims=True)
        self.assertLess(np.abs(unit - unit[0]).max(), 0.03)
        self.assertLess(np.abs(2.0 * px[:, 1] - px[:, 3]).max(), 1.5)
        # bytes are B, G, R, A: the largest level (G) is the largest colour byte
        self.assertEqual(int(px[0, :3].argmax()), 1)
        self.assertGreater(px[0, 0], px[0, 2])  # levels R .1, B .2: B byte above R byte

    def test_louder_levels_are_brighter(self):
        quiet = self.render(self.square(0.5, 1024), levels=np.full((100, 3), 0.05), blocks=1024)
        loud = self.render(self.square(0.5, 1024), levels=np.full((100, 3), 0.5), blocks=1024)
        self.assertGreater(loud[0, 30, 3], quiet[0, 30, 3])

    def test_brightness_follows_how_many_samples_fall_in_the_row(self):
        rng = np.random.default_rng(1)
        x = np.clip(rng.normal(0, 0.1, 1024), -1, 1)
        x[500] = 0.9
        img = self.render(x, blocks=1024)
        a = img[0, :, 3].astype(float)
        last = self.rows(img, 0)[-1]
        self.assertGreater(a[30:33].mean(), 2 * a[last])

    def test_two_level_square_is_a_hollow_box(self):
        # Research 8.2: all samples sit on the two edge rows.
        img = self.render(self.square(0.5, 1024), blocks=1024)
        r = self.rows(img, 0)
        a = img[0, :, 3].astype(float)
        self.assertGreater(a[r[0]], 3 * a[r[len(r) // 2]])
        self.assertGreater(a[r[-1]], 3 * a[r[len(r) // 2]])


class OverviewFit(unittest.TestCase):
    def stripe(self, colour=(0, 0, 200, 200), rows=range(20, 40), cols=range(10)):
        px = np.zeros((1024, 64, 4), np.uint8)
        for c in cols:
            for r in rows:
                px[c, r] = colour
        return pw.Stripe(blocks=1, pixels=px)

    def test_identical_stripes_fit_perfectly(self):
        f = pw.fit_stripes(self.stripe(), self.stripe())
        self.assertEqual(f["colour_error"], 0.0)
        self.assertEqual(f["profile_error"], 0.0)
        self.assertAlmostEqual(f["outline_correlation"], 1.0)

    def test_colour_error_is_distance_between_unit_colours(self):
        a = self.stripe(colour=(200, 0, 0, 200))  # all blue
        b = self.stripe(colour=(0, 200, 0, 200))  # all green
        self.assertAlmostEqual(pw.fit_stripes(a, b)["colour_error"], 2 ** 0.5, places=6)

    def test_profile_error_is_mean_alpha_difference_in_levels(self):
        a = self.stripe(colour=(0, 0, 100, 100))
        b = self.stripe(colour=(0, 0, 110, 110))
        self.assertAlmostEqual(pw.fit_stripes(a, b)["profile_error"], 10.0)

    def test_outline_correlation_follows_the_extents(self):
        a = np.zeros((1024, 64, 4), np.uint8)
        b = a.copy()
        for c in range(1024):
            h = 2 + (c % 20)
            a[c, 30 - h:30 + h] = (0, 0, 9, 9)
            b[c, 30 - h:30 + h] = (0, 0, 9, 9)
        b[:, :, 3] = b[:, ::-1, 3]  # mirrored images have equal extents here
        f = pw.fit_stripes(pw.Stripe(1, a), pw.Stripe(1, b))
        self.assertGreater(f["outline_correlation"], 0.99)
        flat = np.zeros_like(a)
        flat[:, 25:35] = (0, 0, 9, 9)
        f2 = pw.fit_stripes(pw.Stripe(1, a), pw.Stripe(1, flat))
        self.assertLess(f2["outline_correlation"], 0.2)


if __name__ == "__main__":
    unittest.main()
