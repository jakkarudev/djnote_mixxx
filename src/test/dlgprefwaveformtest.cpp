#include <gtest/gtest.h>

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
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

// Only a switch to Phosphor asks for the loaded tracks to be analyzed again.
TEST_F(DlgPrefWaveformTest, OnlySwitchingToPhosphorSignalsTheOverviewSelection) {
    config()->setValue(ConfigKey("[Waveform]", "WaveformOverviewType"), WOverview::Type::RGB);
    WaveformWidgetFactory::instance()->setConfig(config());
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    pDialog->slotUpdate();
    QSignalSpy spy(WaveformWidgetFactory::instance(),
            &WaveformWidgetFactory::phosphorOverviewSelected);
    auto* pCombo = pDialog->waveformOverviewComboBox;
    const auto select = [&](WOverview::Type type) {
        pCombo->setCurrentIndex(pCombo->findData(QVariant::fromValue(type)));
    };
    select(WOverview::Type::Filtered);
    select(WOverview::Type::HSV);
    EXPECT_EQ(0, spy.count());
    select(WOverview::Type::Phosphor);
    EXPECT_EQ(1, spy.count());
    select(WOverview::Type::RGB);
    EXPECT_EQ(1, spy.count());
    select(WOverview::Type::Phosphor);
    EXPECT_EQ(2, spy.count());
}

namespace {
using mixxx::phosphor::PhosphorOverviewSettings;

QDoubleSpinBox* overviewBox(DlgPrefWaveform* pDialog, const char* name) {
    return pDialog->findChild<QDoubleSpinBox*>(QString::fromLatin1(name));
}

PhosphorOverviewSettings nonDefaultOverviewSettings() {
    PhosphorOverviewSettings s;
    s.brightness = 1.7;
    s.levelExponent = 0.5;
    s.densityExponent = 0.4;
    s.gainLow = 1.5;
    s.gainMid = 0.8;
    s.gainHigh = 2.0;
    return s;
}
} // namespace

// The Phosphor overview group shows the stored values and applies edits live.
TEST_F(DlgPrefWaveformTest, PhosphorOverviewGroupShowsAndAppliesTheSettings) {
    mixxx::phosphor::saveOverviewSettings(config(), nonDefaultOverviewSettings());
    WaveformWidgetFactory::instance()->setConfig(config());
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    pDialog->slotUpdate();
    ASSERT_NE(nullptr, overviewBox(pDialog.get(), "phosphorOverviewBrightness"));
    EXPECT_DOUBLE_EQ(1.7, overviewBox(pDialog.get(), "phosphorOverviewBrightness")->value());
    EXPECT_DOUBLE_EQ(0.5, overviewBox(pDialog.get(), "phosphorOverviewLevelExponent")->value());
    EXPECT_DOUBLE_EQ(0.4, overviewBox(pDialog.get(), "phosphorOverviewDensityExponent")->value());
    EXPECT_DOUBLE_EQ(1.5, overviewBox(pDialog.get(), "phosphorOverviewGainLow")->value());
    EXPECT_DOUBLE_EQ(0.8, overviewBox(pDialog.get(), "phosphorOverviewGainMid")->value());
    EXPECT_DOUBLE_EQ(2.0, overviewBox(pDialog.get(), "phosphorOverviewGainHigh")->value());

    QSignalSpy spy(WaveformWidgetFactory::instance(),
            &WaveformWidgetFactory::phosphorOverviewSettingsChanged);
    overviewBox(pDialog.get(), "phosphorOverviewBrightness")->setValue(2.5);
    EXPECT_EQ(1, spy.count());
    EXPECT_DOUBLE_EQ(2.5, WaveformWidgetFactory::instance()->phosphorOverviewSettings().brightness);
    EXPECT_DOUBLE_EQ(2.5, mixxx::phosphor::loadOverviewSettings(config()).brightness);
}

// "Reset to calibrated defaults" resets the overview group only.
TEST_F(DlgPrefWaveformTest, PhosphorOverviewResetButtonResetsOnlyThatGroup) {
    mixxx::phosphor::saveOverviewSettings(config(), nonDefaultOverviewSettings());
    mixxx::phosphor::PhosphorSettings scrolling;
    scrolling.saturation = 7.0;
    mixxx::phosphor::saveSettings(config(), scrolling);
    WaveformWidgetFactory::instance()->setConfig(config());
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    pDialog->slotUpdate();
    auto* pButton = pDialog->findChild<QPushButton*>("phosphorOverviewResetButton");
    ASSERT_NE(nullptr, pButton);
    pButton->click();
    EXPECT_EQ(PhosphorOverviewSettings(),
            WaveformWidgetFactory::instance()->phosphorOverviewSettings());
    EXPECT_EQ(PhosphorOverviewSettings(), mixxx::phosphor::loadOverviewSettings(config()));
    EXPECT_DOUBLE_EQ(1.0, overviewBox(pDialog.get(), "phosphorOverviewBrightness")->value());
    EXPECT_DOUBLE_EQ(7.0, WaveformWidgetFactory::instance()->phosphorSettings().saturation);
}

// Mixxx's own "Reset to defaults" resets the overview group too.
TEST_F(DlgPrefWaveformTest, ResetToDefaultsAlsoResetsThePhosphorOverviewGroup) {
    mixxx::phosphor::saveOverviewSettings(config(), nonDefaultOverviewSettings());
    WaveformWidgetFactory::instance()->setConfig(config());
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    pDialog->slotUpdate();
    pDialog->slotResetToDefaults();
    EXPECT_EQ(PhosphorOverviewSettings(),
            WaveformWidgetFactory::instance()->phosphorOverviewSettings());
    EXPECT_DOUBLE_EQ(0.7, overviewBox(pDialog.get(), "phosphorOverviewLevelExponent")->value());
    EXPECT_DOUBLE_EQ(1.0, overviewBox(pDialog.get(), "phosphorOverviewGainHigh")->value());
}
