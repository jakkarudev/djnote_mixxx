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
