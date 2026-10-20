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

TEST_F(KnobReadoutSettingsTest, ModeDefaultsToSimple) {
    EXPECT_EQ(Mode::Simple, loadSettings(config()).mode);
}

TEST_F(KnobReadoutSettingsTest, ModeRoundTrip) {
    Settings saved;
    saved.mode = Mode::Precise;
    saveSettings(config(), saved);
    EXPECT_EQ(Mode::Precise, loadSettings(config()).mode);

    saved.mode = Mode::Simple;
    saveSettings(config(), saved);
    EXPECT_EQ(Mode::Simple, loadSettings(config()).mode);
}

TEST_F(KnobReadoutSettingsTest, ModeStoredByNameUnderADJNoteKey) {
    Settings saved;
    saved.mode = Mode::Precise;
    saveSettings(config(), saved);

    EXPECT_EQ(QStringLiteral("precise"),
            config()->getValueString(
                    ConfigKey(QStringLiteral("[DJNote]"),
                            QStringLiteral("knobReadoutMode"))));
}

TEST_F(KnobReadoutSettingsTest, UnknownModeFallsBackToSimple) {
    config()->setValue(ConfigKey(QStringLiteral("[DJNote]"),
                               QStringLiteral("knobReadoutMode")),
            QStringLiteral("bogus"));
    EXPECT_EQ(Mode::Simple, loadSettings(config()).mode);
}
