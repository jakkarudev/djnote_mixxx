#include <gtest/gtest.h>

#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "waveform/phosphor/phosphorsettings.h"

namespace {

using namespace mixxx::phosphor;

const ConfigKey kDetailKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorDetail"));

class PhosphorSettingsTest : public MixxxTest {};

} // namespace

TEST(PhosphorSettingsPlainTest, DefaultDetailIsHigh) {
    EXPECT_EQ(WaveformDetail::High, PhosphorSettings().detail);
    EXPECT_EQ(WaveformDetail::High, kDefaultDetail);
}

TEST(PhosphorSettingsPlainTest, TiersKeepBlocksOf1_4_8_16Samples) {
    EXPECT_EQ(1, blockSizeOf(WaveformDetail::Full));
    EXPECT_EQ(4, blockSizeOf(WaveformDetail::High));
    EXPECT_EQ(8, blockSizeOf(WaveformDetail::Medium));
    EXPECT_EQ(16, blockSizeOf(WaveformDetail::Low));
}

TEST(PhosphorSettingsPlainTest, EnumParsingFallsBackToHigh) {
    EXPECT_EQ(WaveformDetail::Full, detailFromInt(0));
    EXPECT_EQ(WaveformDetail::High, detailFromInt(1));
    EXPECT_EQ(WaveformDetail::Medium, detailFromInt(2));
    EXPECT_EQ(WaveformDetail::Low, detailFromInt(3));
    EXPECT_EQ(WaveformDetail::High, detailFromInt(-1));
    EXPECT_EQ(WaveformDetail::High, detailFromInt(4));
    EXPECT_EQ(WaveformDetail::High, detailFromInt(99));
}

TEST(PhosphorSettingsPlainTest, MemoryOfAnEightMinuteTrack) {
    const SINT frames = 8 * 60 * 44100;
    // About 42 / 21 / 10.5 / 5.3 MB, as in the spec.
    EXPECT_NEAR(42.3e6, dataBytes(WaveformDetail::Full, frames), 0.1e6);
    EXPECT_NEAR(21.2e6, dataBytes(WaveformDetail::High, frames), 0.1e6);
    EXPECT_NEAR(10.6e6, dataBytes(WaveformDetail::Medium, frames), 0.1e6);
    EXPECT_NEAR(5.3e6, dataBytes(WaveformDetail::Low, frames), 0.1e6);
}

TEST(PhosphorSettingsPlainTest, CapDropsToTheNextLowerTierUntilItFits) {
    const SINT eightMinutes = 8 * 60 * 44100;
    // Fits: the chosen tier is kept.
    EXPECT_EQ(WaveformDetail::Full, effectiveDetail(WaveformDetail::Full, eightMinutes));
    EXPECT_EQ(WaveformDetail::High, effectiveDetail(WaveformDetail::High, eightMinutes));
    // 8 minutes at 96 kHz: Full would be 92 MB.
    const SINT hiRes = 8 * 60 * 96000;
    EXPECT_EQ(WaveformDetail::High, effectiveDetail(WaveformDetail::Full, hiRes));
    EXPECT_EQ(WaveformDetail::High, effectiveDetail(WaveformDetail::High, hiRes));
    // A 1 hour mix at 44.1 kHz: High is 159 MB, Medium 79 MB.
    const SINT oneHour = 3600 * 44100;
    EXPECT_EQ(WaveformDetail::Medium, effectiveDetail(WaveformDetail::Full, oneHour));
    EXPECT_EQ(WaveformDetail::Medium, effectiveDetail(WaveformDetail::High, oneHour));
    // The floor is the choice: a lower choice is never raised.
    EXPECT_EQ(WaveformDetail::Low, effectiveDetail(WaveformDetail::Low, eightMinutes));
    // Nothing is below Low.
    const SINT sixHours = 6 * 3600 * 44100;
    EXPECT_EQ(WaveformDetail::Low, effectiveDetail(WaveformDetail::Full, sixHours));
}

TEST_F(PhosphorSettingsTest, EmptyConfigGivesDefaults) {
    EXPECT_EQ(WaveformDetail::High, loadSettings(config()).detail);
}

TEST_F(PhosphorSettingsTest, RoundTrip) {
    for (const auto detail : {WaveformDetail::Full,
                 WaveformDetail::High,
                 WaveformDetail::Medium,
                 WaveformDetail::Low}) {
        PhosphorSettings settings;
        settings.detail = detail;
        saveSettings(config(), settings);
        EXPECT_EQ(detail, loadSettings(config()).detail);
    }
}

TEST_F(PhosphorSettingsTest, InvalidValuesFallBackToHigh) {
    config()->setValue(kDetailKey, 17);
    EXPECT_EQ(WaveformDetail::High, loadSettings(config()).detail);
    config()->setValue(kDetailKey, -2);
    EXPECT_EQ(WaveformDetail::High, loadSettings(config()).detail);
}

namespace {
const ConfigKey kSaturationKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorSaturation"));
const ConfigKey kBlendKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorBlend"));
const ConfigKey kGainBKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorGainHigh"));
const ConfigKey kReleaseHighKey(
        QStringLiteral("[Waveform]"), QStringLiteral("PhosphorReleaseHighMs"));
} // namespace

TEST(PhosphorSettingsPlainTest, AdvancedDefaultsAreTheCalibratedValues) {
    const PhosphorSettings s;
    EXPECT_EQ(5.0, s.saturation);
    EXPECT_EQ(1.0, s.channelGains[0]);
    EXPECT_EQ(0.85, s.channelGains[1]);
    EXPECT_EQ(1.8, s.channelGains[2]);
    EXPECT_EQ(0.87, s.brightnessExponent);
    EXPECT_EQ(0.7, s.densityBrightness);
    EXPECT_EQ(LightBlend::Add, s.blend);
    EXPECT_EQ(2.0, s.shapeWindow);
    EXPECT_EQ(0.35, s.color.mixExponent);
    EXPECT_EQ(5.0, s.color.attackMs);
    EXPECT_EQ(350.0, s.color.releaseMs[0]);
    EXPECT_EQ(180.0, s.color.releaseMs[1]);
    EXPECT_EQ(40.0, s.color.releaseMs[2]);
    EXPECT_EQ(120.0, s.color.lookAheadMs);
}

TEST(PhosphorSettingsPlainTest, BlendParsingFallsBackToAdd) {
    EXPECT_EQ(LightBlend::Add, blendFromInt(0));
    EXPECT_EQ(LightBlend::Opaque, blendFromInt(1));
    EXPECT_EQ(LightBlend::Add, blendFromInt(2));
    EXPECT_EQ(LightBlend::Add, blendFromInt(-1));
}

TEST(PhosphorSettingsPlainTest, OutOfRangeValuesAreClamped) {
    PhosphorSettings s;
    s.saturation = 99;
    s.channelGains[0] = -1;
    s.channelGains[2] = 40;
    s.brightnessExponent = 0;
    s.densityBrightness = 50;
    s.shapeWindow = 0.2;
    s.color.mixExponent = 9;
    s.color.attackMs = 500;
    s.color.releaseMs[0] = 9999;
    s.color.releaseMs[1] = -5;
    s.color.lookAheadMs = 5000;
    const PhosphorSettings c = clampSettings(s);
    EXPECT_EQ(10.0, c.saturation);
    EXPECT_EQ(0.0, c.channelGains[0]);
    EXPECT_EQ(4.0, c.channelGains[2]);
    EXPECT_EQ(0.1, c.brightnessExponent);
    EXPECT_EQ(3.0, c.densityBrightness);
    EXPECT_EQ(1.0, c.shapeWindow);
    EXPECT_EQ(1.5, c.color.mixExponent);
    EXPECT_EQ(50.0, c.color.attackMs);
    EXPECT_EQ(2000.0, c.color.releaseMs[0]);
    EXPECT_EQ(0.0, c.color.releaseMs[1]);
    EXPECT_EQ(500.0, c.color.lookAheadMs);
    // In range values are untouched.
    EXPECT_EQ(0.85, clampSettings(PhosphorSettings()).channelGains[1]);
}

TEST_F(PhosphorSettingsTest, AdvancedEmptyConfigGivesDefaults) {
    const PhosphorSettings s = loadSettings(config());
    const PhosphorSettings d;
    EXPECT_EQ(d.saturation, s.saturation);
    EXPECT_EQ(d.channelGains[2], s.channelGains[2]);
    EXPECT_EQ(d.blend, s.blend);
    EXPECT_EQ(d.shapeWindow, s.shapeWindow);
    EXPECT_EQ(d.color.releaseMs[2], s.color.releaseMs[2]);
    EXPECT_EQ(d.color.lookAheadMs, s.color.lookAheadMs);
}

TEST_F(PhosphorSettingsTest, AdvancedRoundTrip) {
    PhosphorSettings s;
    s.saturation = 7.5;
    s.channelGains[1] = 0.2;
    s.brightnessExponent = 1.1;
    s.densityBrightness = 2.25;
    s.blend = LightBlend::Opaque;
    s.shapeWindow = 4.5;
    s.color.mixExponent = 0.6;
    s.color.attackMs = 12;
    s.color.releaseMs[0] = 1000;
    s.color.lookAheadMs = 300;
    saveSettings(config(), s);
    const PhosphorSettings l = loadSettings(config());
    EXPECT_EQ(7.5, l.saturation);
    EXPECT_EQ(0.2, l.channelGains[1]);
    EXPECT_EQ(1.1, l.brightnessExponent);
    EXPECT_EQ(2.25, l.densityBrightness);
    EXPECT_EQ(LightBlend::Opaque, l.blend);
    EXPECT_EQ(4.5, l.shapeWindow);
    EXPECT_EQ(0.6, l.color.mixExponent);
    EXPECT_EQ(12.0, l.color.attackMs);
    EXPECT_EQ(1000.0, l.color.releaseMs[0]);
    EXPECT_EQ(300.0, l.color.lookAheadMs);
}

TEST_F(PhosphorSettingsTest, AdvancedInvalidConfigValuesAreClampedOrDefaulted) {
    config()->setValue(kSaturationKey, 42.0);
    config()->setValue(kGainBKey, -3.0);
    config()->setValue(kReleaseHighKey, 1e9);
    config()->setValue(kBlendKey, 7);
    const PhosphorSettings s = loadSettings(config());
    EXPECT_EQ(10.0, s.saturation);
    EXPECT_EQ(0.0, s.channelGains[2]);
    EXPECT_EQ(2000.0, s.color.releaseMs[2]);
    EXPECT_EQ(LightBlend::Add, s.blend);
    config()->setValue(kSaturationKey, QStringLiteral("not a number"));
    EXPECT_EQ(5.0, loadSettings(config()).saturation);
}

TEST_F(PhosphorSettingsTest, ResetRestoresEveryAdvancedValue) {
    PhosphorSettings s;
    s.saturation = 2;
    s.channelGains[0] = 3;
    s.blend = LightBlend::Opaque;
    s.shapeWindow = 5;
    s.color.releaseMs[1] = 10;
    saveSettings(config(), s);
    // The reset saves a default-constructed PhosphorSettings.
    saveSettings(config(), PhosphorSettings());
    const PhosphorSettings l = loadSettings(config());
    const PhosphorSettings d;
    EXPECT_EQ(d.saturation, l.saturation);
    EXPECT_EQ(d.channelGains[0], l.channelGains[0]);
    EXPECT_EQ(d.blend, l.blend);
    EXPECT_EQ(d.shapeWindow, l.shapeWindow);
    EXPECT_EQ(d.color.releaseMs[1], l.color.releaseMs[1]);
}
