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
