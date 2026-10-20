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
