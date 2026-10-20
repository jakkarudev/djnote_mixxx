#include <gtest/gtest.h>

#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "waveform/scrollinglinesettings.h"

namespace {

using namespace mixxx::scrollinglinelayout;

const QString kGroup = QStringLiteral("[Waveform]");

class ScrollingLineSettingsTest : public MixxxTest {
  protected:
    void setInt(const QString& item, int value) {
        config()->setValue(ConfigKey(kGroup, item), value);
    }
    void setString(const QString& item, const QString& value) {
        config()->setValue(ConfigKey(kGroup, item), value);
    }
};

} // namespace

TEST_F(ScrollingLineSettingsTest, EmptyConfigYieldsDefaults) {
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_TRUE(loaded.enabled);
    EXPECT_EQ(100, loaded.barOpacity.percent());
    EXPECT_EQ(80, loaded.beatOpacity.percent());
}

TEST_F(ScrollingLineSettingsTest, RoundTrip) {
    LineSettings saved;
    saved.enabled = false;
    saved.barOpacity = OpacityPercent(60);
    saved.beatOpacity = OpacityPercent(25);
    saveLineSettings(config(), saved);

    const LineSettings loaded = loadLineSettings(config());
    EXPECT_FALSE(loaded.enabled);
    EXPECT_EQ(60, loaded.barOpacity.percent());
    EXPECT_EQ(25, loaded.beatOpacity.percent());
}

TEST_F(ScrollingLineSettingsTest, OutOfRangeOpacitiesAreClamped) {
    setInt(QStringLiteral("scrollingBarLineOpacity"), 400);
    setInt(QStringLiteral("scrollingBeatLineOpacity"), -3);
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(100, loaded.barOpacity.percent());
    EXPECT_EQ(0, loaded.beatOpacity.percent());
}

TEST_F(ScrollingLineSettingsTest, UpstreamBeatGridAlphaIsNotWritten) {
    saveLineSettings(config(), LineSettings());
    EXPECT_FALSE(config()->exists(ConfigKey(kGroup, QStringLiteral("beatGridAlpha"))));
}

TEST_F(ScrollingLineSettingsTest, EmptyConfigYieldsDefaultWidthsAndSkinColours) {
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(2, loaded.barWidth.px());
    EXPECT_EQ(1, loaded.beatWidth.px());
    EXPECT_FALSE(loaded.barColor.has_value());
    EXPECT_FALSE(loaded.beatColor.has_value());
}

TEST_F(ScrollingLineSettingsTest, WidthsAndColoursRoundTrip) {
    LineSettings saved;
    saved.barWidth = LineWidthPx(4);
    saved.beatWidth = LineWidthPx(3);
    saved.barColor = QColor(0x12, 0x34, 0x56);
    saveLineSettings(config(), saved);

    LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(4, loaded.barWidth.px());
    EXPECT_EQ(3, loaded.beatWidth.px());
    ASSERT_TRUE(loaded.barColor.has_value());
    EXPECT_EQ(QColor(0x12, 0x34, 0x56), *loaded.barColor);
    EXPECT_FALSE(loaded.beatColor.has_value());

    // Back to the skin colour.
    saved.barColor.reset();
    saved.beatColor = QColor(0xab, 0xcd, 0xef);
    saveLineSettings(config(), saved);
    loaded = loadLineSettings(config());
    EXPECT_FALSE(loaded.barColor.has_value());
    ASSERT_TRUE(loaded.beatColor.has_value());
    EXPECT_EQ(QColor(0xab, 0xcd, 0xef), *loaded.beatColor);
}

TEST_F(ScrollingLineSettingsTest, OutOfRangeWidthsAreClamped) {
    setInt(QStringLiteral("scrollingBarLineWidth"), 12);
    setInt(QStringLiteral("scrollingBeatLineWidth"), 0);
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_EQ(5, loaded.barWidth.px());
    EXPECT_EQ(1, loaded.beatWidth.px());
}

TEST_F(ScrollingLineSettingsTest, InvalidColourFallsBackToTheSkinColour) {
    setString(QStringLiteral("scrollingBarLineColor"), QStringLiteral("not a colour"));
    setString(QStringLiteral("scrollingBeatLineColor"), QString());
    const LineSettings loaded = loadLineSettings(config());
    EXPECT_FALSE(loaded.barColor.has_value());
    EXPECT_FALSE(loaded.beatColor.has_value());
}
