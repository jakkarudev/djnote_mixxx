#include <gtest/gtest.h>

#include <memory>

#include "preferences/dialog/dlgprefwaveform.h"
#include "test/mixxxtest.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/woverview.h"

class DlgPrefWaveformTest : public MixxxTest {
  protected:
    void SetUp() override {
        WaveformWidgetFactory::createInstance();
        WaveformWidgetFactory::instance()->setConfig(config());
    }
    void TearDown() override {
        WaveformWidgetFactory::destroy();
    }
};

// The Phosphor controls are built while the dialog is constructed; showing the
// stored settings must not run before they exist (it crashed Mixxx at startup).
TEST_F(DlgPrefWaveformTest, ConstructsAndShowsThePhosphorSettings) {
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    EXPECT_TRUE(pDialog);
}

// Phosphor is the fourth overview type; the stored indices of the others stay.
TEST_F(DlgPrefWaveformTest, PhosphorIsListedAfterTheOtherOverviewTypes) {
    EXPECT_EQ(0, static_cast<int>(WOverview::Type::Filtered));
    EXPECT_EQ(1, static_cast<int>(WOverview::Type::HSV));
    EXPECT_EQ(2, static_cast<int>(WOverview::Type::RGB));
    EXPECT_EQ(3, static_cast<int>(WOverview::Type::Phosphor));

    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    const auto* pCombo = pDialog->waveformOverviewComboBox;
    ASSERT_EQ(4, pCombo->count());
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(i, static_cast<int>(pCombo->itemData(i).value<WOverview::Type>()));
    }
}
