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

    def test_outline_is_identical_at_every_tier_when_the_window_is_32_frames_or_more(self):
        rng = np.random.default_rng(1)
        noise = (rng.uniform(-1, 1, 4000) * 20000).astype(np.int16)
        square = np.where((np.arange(4000) // 37) % 2, 15000, -15000).astype(np.int16)
        # spp 40 and 17 (odd, windows of 80 and 34 frames), fractional positions.
        for mono in (noise, square):
            for spp, pos in ((40.0, 2000.3), (17.0, 1999.7), (16.0, 2103.0)):
                full = self.lane(mono, spp, pos, tier=1, width=20)
                for tier in (4, 8, 16):
                    t = self.lane(mono, spp, pos, tier=tier, width=20)
                    np.testing.assert_array_equal(full.status, t.status)
                    np.testing.assert_array_equal(full.row_min, t.row_min)
                    np.testing.assert_array_equal(full.row_max, t.row_max)

    def test_window_snaps_outward_to_blocks_of_16_frames(self):
        # A single spike at frame 70; a window of 40 frames around 50 reaches
        # frames 30..70 and so snaps to 16..80 and sees the spike at 70, and a
        # spike at 79 is seen too, while one at 80 is not.
        for spike, seen in ((70, True), (79, True), (80, False), (15, False), (16, True)):
            mono = np.zeros(200, np.int16)
            mono[spike] = 20000
            lane = self.lane(mono, 20.0, 50.0, width=1, window=2.0)
            self.assertEqual(lane.row_max[0] > (self.H - 1) / 2 + 1, seen, spike)

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
        n = blocks or -(-len(mono) // 1920)
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


class OverviewColumns(unittest.TestCase):
    def columns(self, mono, rate=44100, levels=None):
        mono = np.asarray(mono, np.float64)
        if levels is None:
            levels = np.tile([0.1, 0.3, 0.5], (len(mono) // 44 + 1, 1))
        return pw.overview_columns(mono, rate, pw.default_args(), levels=levels)

    def test_1920_columns_of_ceil_frames_over_1920_samples(self):
        c = self.columns(np.full(1920 * 3 + 1, 0.5))
        self.assertEqual(c.n, 4)
        self.assertEqual(c.levels.shape, (1920, 3))
        self.assertEqual(c.density.shape, (1920, 62))
        self.assertEqual(c.levels.dtype, np.uint8)
        self.assertEqual(c.density.dtype, np.uint8)

    def test_columns_past_the_end_are_empty(self):
        c = pw.overview_columns(np.full(2000, 0.5), 44100, pw.default_args(),
                                levels=np.full((100, 3), 0.1), n=1000)
        self.assertTrue(c.density[:2].any())
        self.assertFalse(c.density[2:].any())
        self.assertFalse(c.levels[2:].any())

    def test_density_byte_is_the_quartic_root_of_share_over_0_8(self):
        # 3 of 4 samples in row 46 (v = 0.5), 1 of 4 in row 15 (v = -0.5).
        c = self.columns(np.tile([0.5, 0.5, 0.5, -0.5], 1920))
        col = c.density[0]
        self.assertEqual(col[46], round(255 * (0.75 / 0.8) ** 0.25))
        self.assertEqual(col[15], round(255 * (0.25 / 0.8) ** 0.25))
        self.assertEqual(np.count_nonzero(col), 2)  # gaps stay 0: the renderer adds the floor

    def test_density_is_clipped_to_255(self):
        c = self.columns(np.full(1920, 0.5))  # share 1.0 > 0.8
        self.assertEqual(c.density[0, 46], 255)

    def test_levels_are_the_peak_hold_over_the_column_as_bytes(self):
        levels = np.zeros((1920 * 100 + 2, 3))
        levels[10] = (0.2, 0.9, 0.4)
        mono = np.full(1920 * 4410, 0.1)  # N = 4410 -> 100 colour blocks per column
        c = pw.overview_columns(mono, 44100, pw.default_args(), levels=levels)
        self.assertEqual(c.n, 4410)
        self.assertEqual(c.levels[0].tolist(), [51, 230, 102])
        self.assertEqual(c.levels[1].tolist(), [0, 0, 0])

    def test_digital_silence_is_a_one_row_line_at_row_31(self):
        c = self.columns(np.zeros(1920 * 2))
        self.assertEqual(np.nonzero(c.density[0])[0].tolist(), [31])

    def test_real_colour_track_levels_follow_a_tone(self):
        t = np.arange(44100 * 2) / 44100
        mono = 0.5 * np.sin(2 * np.pi * 100 * t)
        c = pw.overview_columns(mono, 44100, pw.default_args())
        mid = c.levels[960]
        self.assertGreater(mid[0], mid[2])  # 100 Hz is mostly red (low)


class OverviewImage(unittest.TestCase):
    def cols(self, samples, levels=(0.1, 0.4, 0.2)):
        samples = np.asarray(samples, np.float64)
        lv = np.tile(levels, (len(samples) * 1920 // 44 + 1, 1))
        return pw.overview_columns(np.tile(samples, 1920), 44100, pw.default_args(), levels=lv)

    def square(self, amp, n=1024):
        x = np.full(n, amp)
        x[1::2] = -amp
        return x

    def image(self, c, height=62, **kw):
        s = pw.OverviewSettings(**kw)
        return pw.overview_image(c.levels, c.density, c.n, s, height)

    def rows(self, img, col):
        return np.nonzero(img[:, col].any(1))[0]

    W = np.array([83.5, 242.0, -37.4, 606.1])

    def magnitude(self, level_bytes, d):
        lv = level_bytes / 255.0
        norm = np.linalg.norm(lv)
        u = lv / norm
        return u, float(self.W @ np.append(u, u[2] ** 4) * norm ** 0.7 * d ** 0.25)

    def test_shape_is_height_by_1920_bgra(self):
        c = self.cols(self.square(0.5))
        self.assertEqual(self.image(c).shape, (62, 1920, 4))
        self.assertEqual(self.image(c, height=31).shape, (31, 1920, 4))

    def test_defaults_match_the_model(self):
        s = pw.OverviewSettings()
        self.assertEqual((s.brightness, s.level_exponent, s.density_exponent), (1.0, 0.7, 0.25))
        self.assertEqual((s.gain_low, s.gain_mid, s.gain_high), (1.0, 1.0, 1.0))

    def test_extent_rows_and_hollow_square(self):
        img = self.image(self.cols(self.square(0.5)))
        r = self.rows(img, 0)
        self.assertEqual((r[0], r[-1]), (15, 46))
        a = img[:, 0, 3].astype(float)
        self.assertGreater(a[15], 3 * a[30])  # hollow: edges bright

    def test_pixel_value_follows_the_stored_density_byte_and_the_formula(self):
        c = self.cols(self.square(0.5))
        img = self.image(c)
        d = 0.8 * (c.density[0, 15] / 255.0) ** 4
        u, mag = self.magnitude(c.levels[0].astype(float), d)
        want = np.clip(np.floor(u * mag + 0.5), 0, 255)
        self.assertEqual(img[15, 0, :3].tolist(), [want[2], want[1], want[0]])
        self.assertEqual(img[15, 0, 3], min(255, int(np.floor(2 * u[1] * mag + 0.5))))

    def test_gaps_inside_the_extent_get_half_a_sample(self):
        c = self.cols(self.square(0.5))
        img = self.image(c)
        u, mag = self.magnitude(c.levels[0].astype(float), 0.5 / c.n)
        self.assertEqual(img[30, 0, 1], int(np.floor(u[1] * mag + 0.5)))

    def test_nothing_outside_the_extent(self):
        img = self.image(self.cols(self.square(0.5)))
        self.assertFalse(img[:15, 0].any())
        self.assertFalse(img[47:, 0].any())

    def test_columns_done_leaves_the_rest_empty(self):
        c = self.cols(self.square(0.5))
        img = pw.overview_image(c.levels, c.density, c.n, pw.OverviewSettings(), 62, columns_done=100)
        self.assertTrue(img[:, 99].any())
        self.assertFalse(img[:, 100:].any())

    def test_silence_is_an_opaque_black_centre_line(self):
        c = pw.overview_columns(np.zeros(1920 * 2), 44100, pw.default_args(),
                                levels=np.zeros((100, 3)))
        img = self.image(c)
        self.assertEqual(self.rows(img, 0).tolist(), [31])
        self.assertEqual(img[31, 0].tolist(), [0, 0, 0, 255])

    def test_brightness_scales_the_colour_and_clips(self):
        c = self.cols(self.square(0.5))
        a = self.image(c, brightness=0.5)[15, 0, :3].astype(float)
        b = self.image(c, brightness=1.0)[15, 0, :3].astype(float)
        self.assertLess(np.abs(a * 2 - b).max(), 2)
        self.assertEqual(self.image(c, brightness=4.0)[15, 0, :3].max(), 255)

    def test_gains_change_the_hue_and_zero_gains_leave_a_black_line(self):
        c = self.cols(self.square(0.5))
        base = self.image(c)[15, 0]
        red = self.image(c, gain_low=4.0)[15, 0]
        self.assertGreater(int(red[2]) / max(1, int(red[1])), 2 * int(base[2]) / int(base[1]))
        off = self.image(c, gain_low=0, gain_mid=0, gain_high=0)
        self.assertEqual(off[15, 0].tolist(), [0, 0, 0, 255])

    def test_density_exponent_one_makes_the_pixel_proportional_to_density(self):
        rng = np.random.default_rng(3)
        x = np.clip(rng.normal(0, 0.1, 1024), -1, 1)
        c = self.cols(x)
        img = self.image(c, density_exponent=1.0).astype(float)
        d = 0.8 * (c.density[0] / 255.0) ** 4
        ratio = d[31] / d[33]
        self.assertLess(abs(img[31, 0, 1] / img[33, 0, 1] - ratio), 0.05 * ratio)

    def test_height_resampling_averages_source_rows(self):
        c = self.cols(self.square(0.5))
        full = self.image(c).astype(float)
        half = self.image(c, height=31).astype(float)
        want = (full[0:62:2] + full[1:62:2]) / 2
        self.assertLess(np.abs(half - want).max(), 1.01)
        double = self.image(c, height=124).astype(float)
        self.assertLess(np.abs(double[0::2] - full).max(), 0.01)
        self.assertLess(np.abs(double[1::2] - full).max(), 0.01)


class OverviewFixtures(unittest.TestCase):
    def build(self):
        t = np.arange(44100 * 2)
        left = (0.5 * 32767 * np.sin(2 * np.pi * 440 * t / 44100)).astype(np.int16)
        return pw.build_overview_fixture("tone", 44100, left, left,
                                         [("default_h62", pw.OverviewSettings(), 62, None),
                                          ("dim_h31", pw.OverviewSettings(brightness=0.5), 31, 900)])

    def test_is_deterministic(self):
        self.assertEqual(self.build(), self.build())

    def test_contains_input_columns_and_images_that_match_the_functions(self):
        f = pw.read_overview_fixture(self.build())
        self.assertEqual(f["frames"], 88200)
        self.assertEqual(f["channels"], 1)
        self.assertEqual(f["samples"].shape, (88200, 1))
        mono = pw.mono_int16(f["samples"][:, 0], f["samples"][:, 0]) / 32768.0
        cols = pw.overview_columns(mono, 44100, pw.default_args())
        self.assertEqual(f["n"], cols.n)
        self.assertTrue((f["levels"] == cols.levels).all())
        self.assertTrue((f["density"] == cols.density).all())
        a, b = f["images"]
        self.assertEqual((a["name"], a["height"], a["columns_done"]), ("default_h62", 62, 1920))
        self.assertEqual(b["settings"]["brightness"], 0.5)
        self.assertEqual(b["columns_done"], 900)
        want = pw.overview_image(f["levels"], f["density"], f["n"],
                                 pw.OverviewSettings(brightness=0.5), 31, columns_done=900)
        self.assertTrue((b["pixels"] == want).all())


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
