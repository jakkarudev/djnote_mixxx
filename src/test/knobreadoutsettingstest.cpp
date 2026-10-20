#include <gtest/gtest.h>

#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "widget/knobreadoutsettings.h"

// Loading and saving of the knob adjustment readout settings (ticket #67).
namespace {

using namespace mixxx::knobreadout;

class KnobReadoutSettingsTest : public MixxxTest {};

} // namespace

TEST_F(KnobReadoutSettingsTest, EmptyConfigYieldsDefaults) {
    const Settings loaded = loadSettings(config());
    EXPECT_TRUE(loaded.enabled);
}

TEST_F(KnobReadoutSettingsTest, RoundTrip) {
    Settings saved;
    saved.enabled = false;
    saveSettings(config(), saved);

    EXPECT_FALSE(loadSettings(config()).enabled);
}

TEST_F(KnobReadoutSettingsTest, StoredUnderADJNoteKey) {
    Settings saved;
    saved.enabled = false;
    saveSettings(config(), saved);

    EXPECT_EQ(QStringLiteral("0"),
            config()->getValueString(
                    ConfigKey(QStringLiteral("[DJNote]"),
                            QStringLiteral("knobReadoutEnabled"))));
}

// Style (ticket #70): Under the knob (default) or On the knob face.
TEST_F(KnobReadoutSettingsTest, StyleDefaultsToUnderTheKnob) {
    EXPECT_EQ(Style::UnderKnob, loadSettings(config()).style);
}

TEST_F(KnobReadoutSettingsTest, StyleRoundTrip) {
    Settings saved;
    saved.style = Style::KnobFace;
    saveSettings(config(), saved);
    EXPECT_EQ(Style::KnobFace, loadSettings(config()).style);

    saved.style = Style::UnderKnob;
    saveSettings(config(), saved);
    EXPECT_EQ(Style::UnderKnob, loadSettings(config()).style);
}

TEST_F(KnobReadoutSettingsTest, StyleStoredUnderADJNoteKey) {
    Settings saved;
    saved.style = Style::KnobFace;
    saveSettings(config(), saved);

    EXPECT_EQ(QStringLiteral("knobFace"),
            config()->getValueString(
                    ConfigKey(QStringLiteral("[DJNote]"),
                            QStringLiteral("knobReadoutStyle"))));
}

TEST_F(KnobReadoutSettingsTest, UnknownStyleFallsBackToTheDefault) {
    config()->setValue(ConfigKey(QStringLiteral("[DJNote]"),
                               QStringLiteral("knobReadoutStyle")),
            QStringLiteral("sideways"));
    EXPECT_EQ(Style::UnderKnob, loadSettings(config()).style);
}
