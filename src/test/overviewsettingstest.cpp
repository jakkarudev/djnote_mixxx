#include <gtest/gtest.h>

#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "waveform/overviewsettings.h"

namespace {

using namespace mixxx::overviewlayout;

const QString kGroup = QStringLiteral("[Waveform]");

class OverviewSettingsTest : public MixxxTest {
  protected:
    void setInt(const QString& item, int value) {
        config()->setValue(ConfigKey(kGroup, item), value);
    }
};

} // namespace

TEST(OverviewSettingsDefaultsTest, PhraseBarsChoicesIncludeTheDefault) {
    EXPECT_EQ(kDefaultPhraseBars, validPhraseBarsOrDefault(kDefaultPhraseBars));
    for (const int bars : kPhraseBarsChoices) {
        EXPECT_EQ(bars, validPhraseBarsOrDefault(bars));
    }
    EXPECT_EQ(kDefaultPhraseBars, validPhraseBarsOrDefault(5));
    EXPECT_EQ(kDefaultPhraseBars, validPhraseBarsOrDefault(0));
    EXPECT_EQ(kDefaultPhraseBars, LineSettings().phraseBars);
}

TEST(OverviewSettingsDefaultsTest, OpacityIsClampedAndHasDefault) {
    EXPECT_EQ(80, OpacityPercent().percent());
    EXPECT_EQ(0, OpacityPercent(-5).percent());
    EXPECT_EQ(100, OpacityPercent(250).percent());
    EXPECT_EQ(42, OpacityPercent(42).percent());
    EXPECT_DOUBLE_EQ(0.42, OpacityPercent(42).fraction());
}

TEST_F(OverviewSettingsTest, EmptyConfigYieldsDefaults) {
    const LineSettings lines = loadLineSettings(config());
    EXPECT_EQ(LineSettings().mode, lines.mode);
    EXPECT_EQ(LineSettings().phraseBars, lines.phraseBars);
    EXPECT_EQ(LineSettings().opacity.percent(), lines.opacity.percent());

    const HoverReadoutSettings hover = loadHoverReadoutSettings(config());
    const HoverReadoutSettings defaults;
    EXPECT_EQ(defaults.showHoverReadout, hover.showHoverReadout);
    EXPECT_EQ(defaults.display, hover.display);
    EXPECT_EQ(defaults.hoverMode, hover.hoverMode);
    EXPECT_EQ(OverCue::JoinCueTimes, hover.overCue);
    EXPECT_EQ(defaults.unit, hover.unit);
    EXPECT_EQ(defaults.precision, hover.precision);
    EXPECT_EQ(defaults.position, hover.position);
}

TEST_F(OverviewSettingsTest, LineSettingsRoundTrip) {
    LineSettings saved;
    saved.mode = LinesMode::BarsAndPhrases;
    saved.phraseBars = 16;
    saved.opacity = OpacityPercent(35);
    saveLineSettings(config(), saved);

    const LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(LinesMode::BarsAndPhrases, loaded.mode);
    EXPECT_EQ(16, loaded.phraseBars);
    EXPECT_EQ(35, loaded.opacity.percent());
}

TEST_F(OverviewSettingsTest, InvalidLineSettingsFallBack) {
    setInt(QStringLiteral("overviewLinesMode"), 7);
    setInt(QStringLiteral("overviewPhraseBars"), 6);
    setInt(QStringLiteral("overviewLineOpacity"), 400);
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(LineSettings().mode, loaded.mode);
    EXPECT_EQ(kDefaultPhraseBars, loaded.phraseBars);
    EXPECT_EQ(100, loaded.opacity.percent());
}

TEST_F(OverviewSettingsTest, HoverReadoutSettingsRoundTrip) {
    HoverReadoutSettings saved;
    saved.showHoverReadout = false;
    saved.display = HoverDisplay::LabelOnly;
    saved.hoverMode = HoverMode::SeparateHoverLine;
    saved.overCue = OverCue::MoveHoverReadout;
    saved.unit = BeatDistanceUnit::Beats;
    saved.precision = BeatDistancePrecision::Fractional;
    saved.position = HoverReadoutPosition::AlwaysLeft;
    saveHoverReadoutSettings(config(), saved);

    const HoverReadoutSettings loaded = loadHoverReadoutSettings(config());
    EXPECT_FALSE(loaded.showHoverReadout);
    EXPECT_EQ(HoverDisplay::LabelOnly, loaded.display);
    EXPECT_EQ(HoverMode::SeparateHoverLine, loaded.hoverMode);
    EXPECT_EQ(OverCue::MoveHoverReadout, loaded.overCue);
    EXPECT_EQ(BeatDistanceUnit::Beats, loaded.unit);
    EXPECT_EQ(BeatDistancePrecision::Fractional, loaded.precision);
    EXPECT_EQ(HoverReadoutPosition::AlwaysLeft, loaded.position);
}

TEST_F(OverviewSettingsTest, InvalidHoverReadoutSettingsFallBackIndividually) {
    setInt(QStringLiteral("overviewHoverUnit"), 9);
    setInt(QStringLiteral("overviewHoverPrecision"), -1);
    setInt(QStringLiteral("overviewHoverLabelPosition"), 2);
    setInt(QStringLiteral("overviewHoverDisplay"), 3);
    setInt(QStringLiteral("overviewHoverMode"), 1);
    const HoverReadoutSettings loaded = loadHoverReadoutSettings(config());
    const HoverReadoutSettings defaults;
    EXPECT_EQ(defaults.unit, loaded.unit);
    EXPECT_EQ(defaults.precision, loaded.precision);
    EXPECT_EQ(HoverReadoutPosition::AlwaysLeft, loaded.position);
    EXPECT_EQ(defaults.display, loaded.display);
    EXPECT_EQ(HoverMode::SeparateHoverLine, loaded.hoverMode);
}

TEST_F(OverviewSettingsTest, OverCueIsStoredUnderItsOwnKeyAndValidated) {
    HoverReadoutSettings saved;
    saved.overCue = OverCue::HideCueTimes;
    saveHoverReadoutSettings(config(), saved);
    EXPECT_EQ(static_cast<int>(OverCue::HideCueTimes),
            config()->getValue(ConfigKey(kGroup, QStringLiteral("overviewHoverOverCue")), -1));

    setInt(QStringLiteral("overviewHoverOverCue"), 4);
    EXPECT_EQ(OverCue::JoinCueTimes, loadHoverReadoutSettings(config()).overCue);
    setInt(QStringLiteral("overviewHoverOverCue"), -1);
    EXPECT_EQ(OverCue::JoinCueTimes, loadHoverReadoutSettings(config()).overCue);
}
