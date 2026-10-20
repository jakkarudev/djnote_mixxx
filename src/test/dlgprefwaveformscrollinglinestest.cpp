#include <gtest/gtest.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <memory>
#include <vector>

#include "preferences/dialog/dlgprefwaveform.h"
#include "test/mixxxtest.h"
#include "waveform/scrollinglinesettings.h"
#include "waveform/waveformwidgetfactory.h"

// The "Beat, bar and step lines" group on the Waveforms page (tickets #63, #64, #65).
namespace {

using mixxx::scrollinglinelayout::LineSettings;
using mixxx::scrollinglinelayout::LineWidthPx;
using mixxx::scrollinglinelayout::OpacityPercent;

class DlgPrefWaveformScrollingLinesTest : public MixxxTest {
  protected:
    void SetUp() override {
        WaveformWidgetFactory::createInstance();
        WaveformWidgetFactory::instance()->setConfig(config());
    }
    void TearDown() override {
        WaveformWidgetFactory::destroy();
    }

    std::unique_ptr<DlgPrefWaveform> openPage() {
        auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
        pDialog->slotUpdate();
        return pDialog;
    }

    template<typename T>
    static T* control(DlgPrefWaveform* pDialog, const char* name) {
        T* pControl = pDialog->findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(nullptr, pControl) << name;
        return pControl;
    }

    // Picks a custom colour the way the user does: the colour button opens a
    // colour dialog, in which a colour is chosen and accepted.
    static void pickColor(DlgPrefWaveform* pDialog, const char* buttonName, const QColor& color) {
        auto* pButton = control<QPushButton>(pDialog, buttonName);
        ASSERT_NE(nullptr, pButton);
        pButton->click();
        QColorDialog* pColorDialog = nullptr;
        for (auto* pCandidate : pDialog->findChildren<QColorDialog*>()) {
            if (pCandidate->isVisible()) {
                pColorDialog = pCandidate;
            }
        }
        ASSERT_NE(nullptr, pColorDialog);
        ASSERT_TRUE(pColorDialog->isVisible());
        pColorDialog->setCurrentColor(color);
        pColorDialog->accept();
    }

    static const LineSettings& factorySettings() {
        return WaveformWidgetFactory::instance()->getScrollingLineSettings();
    }

    static void storeNonDefaults(UserSettingsPointer pConfig) {
        LineSettings settings;
        settings.enabled = false;
        settings.barOpacity = OpacityPercent(55);
        settings.beatOpacity = OpacityPercent(30);
        settings.barWidth = LineWidthPx(4);
        settings.beatWidth = LineWidthPx(3);
        settings.barColor = QColor(0x11, 0x22, 0x33);
        mixxx::scrollinglinelayout::saveLineSettings(pConfig, settings);
        WaveformWidgetFactory::instance()->setConfig(pConfig);
    }
};

} // namespace

TEST_F(DlgPrefWaveformScrollingLinesTest, GroupShowsTheDefaults) {
    auto pDialog = openPage();
    auto* pGroup = control<QGroupBox>(pDialog.get(), "scrollingLinesGroupBox");
    ASSERT_NE(nullptr, pGroup);
    EXPECT_EQ(QStringLiteral("Beat, bar and step lines"), pGroup->title());
    auto* pEnabled = control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox");
    auto* pBar = control<QSpinBox>(pDialog.get(), "scrollingBarLineOpacitySpinBox");
    auto* pBeat = control<QSpinBox>(pDialog.get(), "scrollingBeatLineOpacitySpinBox");
    ASSERT_TRUE(pEnabled && pBar && pBeat);
    EXPECT_TRUE(pEnabled->isChecked());
    EXPECT_EQ(100, pBar->value());
    EXPECT_EQ(80, pBeat->value());
    EXPECT_EQ(100, control<QSlider>(pDialog.get(), "scrollingBarLineOpacitySlider")->value());
    EXPECT_EQ(80, control<QSlider>(pDialog.get(), "scrollingBeatLineOpacitySlider")->value());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, GroupShowsTheStoredSettings) {
    storeNonDefaults(config());
    auto pDialog = openPage();
    EXPECT_FALSE(control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox")->isChecked());
    EXPECT_EQ(55, control<QSpinBox>(pDialog.get(), "scrollingBarLineOpacitySpinBox")->value());
    EXPECT_EQ(30, control<QSlider>(pDialog.get(), "scrollingBeatLineOpacitySlider")->value());
}

// Edits reach the waveform widget factory at once (the renderers read it every
// frame) and are stored.
TEST_F(DlgPrefWaveformScrollingLinesTest, EditsApplyLiveAndPersist) {
    auto pDialog = openPage();
    control<QSlider>(pDialog.get(), "scrollingBarLineOpacitySlider")->setValue(40);
    EXPECT_EQ(40, control<QSpinBox>(pDialog.get(), "scrollingBarLineOpacitySpinBox")->value());
    EXPECT_EQ(40, factorySettings().barOpacity.percent());

    control<QSpinBox>(pDialog.get(), "scrollingBeatLineOpacitySpinBox")->setValue(15);
    EXPECT_EQ(15, control<QSlider>(pDialog.get(), "scrollingBeatLineOpacitySlider")->value());
    EXPECT_EQ(15, factorySettings().beatOpacity.percent());

    control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox")->setChecked(false);
    EXPECT_FALSE(factorySettings().enabled);

    const LineSettings stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    EXPECT_FALSE(stored.enabled);
    EXPECT_EQ(40, stored.barOpacity.percent());
    EXPECT_EQ(15, stored.beatOpacity.percent());
}

// While the master switch is on, upstream's beat grid opacity controls are
// greyed out (still visible, value untouched); off, they work normally.
TEST_F(DlgPrefWaveformScrollingLinesTest, MasterSwitchGreysOutUpstreamBeatGridOpacity) {
    auto pDialog = openPage();
    auto* pEnabled = control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox");
    const int upstreamAlpha = pDialog->beatGridAlphaSpinBox->value();
    EXPECT_FALSE(pDialog->beatGridAlphaSlider->isEnabled());
    EXPECT_FALSE(pDialog->beatGridAlphaSpinBox->isEnabled());
    EXPECT_FALSE(pDialog->beatGridAlphaSlider->isHidden());

    pEnabled->setChecked(false);
    EXPECT_TRUE(pDialog->beatGridAlphaSlider->isEnabled());
    EXPECT_TRUE(pDialog->beatGridAlphaSpinBox->isEnabled());

    pEnabled->setChecked(true);
    EXPECT_FALSE(pDialog->beatGridAlphaSlider->isEnabled());
    EXPECT_FALSE(pDialog->beatGridAlphaSpinBox->isEnabled());
    EXPECT_EQ(upstreamAlpha, pDialog->beatGridAlphaSpinBox->value());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, StoredSwitchOffLeavesUpstreamBeatGridOpacityEnabled) {
    storeNonDefaults(config());
    auto pDialog = openPage();
    EXPECT_TRUE(pDialog->beatGridAlphaSlider->isEnabled());
    EXPECT_TRUE(pDialog->beatGridAlphaSpinBox->isEnabled());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, ResetToDefaultsResetsTheGroup) {
    storeNonDefaults(config());
    auto pDialog = openPage();
    pDialog->slotResetToDefaults();
    EXPECT_TRUE(control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox")->isChecked());
    EXPECT_EQ(100, control<QSpinBox>(pDialog.get(), "scrollingBarLineOpacitySpinBox")->value());
    EXPECT_EQ(80, control<QSlider>(pDialog.get(), "scrollingBeatLineOpacitySlider")->value());
    EXPECT_TRUE(factorySettings().enabled);
    EXPECT_EQ(100, factorySettings().barOpacity.percent());
    EXPECT_EQ(80, factorySettings().beatOpacity.percent());
    EXPECT_TRUE(mixxx::scrollinglinelayout::loadLineSettings(config()).enabled);
    EXPECT_FALSE(pDialog->beatGridAlphaSlider->isEnabled());
}

// The group never writes upstream's beat grid opacity key.
TEST_F(DlgPrefWaveformScrollingLinesTest, EditsDoNotWriteUpstreamBeatGridAlpha) {
    auto pDialog = openPage();
    config()->remove(ConfigKey("[Waveform]", "beatGridAlpha"));
    control<QCheckBox>(pDialog.get(), "scrollingLinesEnabledCheckBox")->setChecked(false);
    control<QSpinBox>(pDialog.get(), "scrollingBeatLineOpacitySpinBox")->setValue(10);
    EXPECT_FALSE(config()->exists(ConfigKey("[Waveform]", "beatGridAlpha")));
}

TEST_F(DlgPrefWaveformScrollingLinesTest, GroupShowsDefaultWidthsAndSkinColours) {
    auto pDialog = openPage();
    auto* pBarWidth = control<QSpinBox>(pDialog.get(), "scrollingBarLineWidthSpinBox");
    auto* pBeatWidth = control<QSpinBox>(pDialog.get(), "scrollingBeatLineWidthSpinBox");
    ASSERT_TRUE(pBarWidth && pBeatWidth);
    EXPECT_EQ(2, pBarWidth->value());
    EXPECT_EQ(1, pBeatWidth->value());
    EXPECT_EQ(1, pBarWidth->minimum());
    EXPECT_EQ(5, pBarWidth->maximum());
    for (const char* name : {"scrollingBarLineColorButton", "scrollingBeatLineColorButton"}) {
        auto* pButton = control<QPushButton>(pDialog.get(), name);
        ASSERT_NE(nullptr, pButton);
        EXPECT_EQ(QStringLiteral("Skin colour"), pButton->text()) << name;
    }
    // Already on the skin colour: nothing to go back to.
    EXPECT_FALSE(control<QPushButton>(pDialog.get(), "scrollingBarLineSkinColorButton")
                         ->isEnabled());
    EXPECT_FALSE(control<QPushButton>(pDialog.get(), "scrollingBeatLineSkinColorButton")
                         ->isEnabled());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, GroupShowsStoredWidthsAndColours) {
    storeNonDefaults(config());
    auto pDialog = openPage();
    EXPECT_EQ(4, control<QSpinBox>(pDialog.get(), "scrollingBarLineWidthSpinBox")->value());
    EXPECT_EQ(3, control<QSpinBox>(pDialog.get(), "scrollingBeatLineWidthSpinBox")->value());
    EXPECT_NE(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingBarLineColorButton")->text());
    EXPECT_TRUE(control<QPushButton>(pDialog.get(), "scrollingBarLineSkinColorButton")
                        ->isEnabled());
    EXPECT_EQ(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingBeatLineColorButton")->text());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, WidthEditsApplyLiveAndPersist) {
    auto pDialog = openPage();
    control<QSpinBox>(pDialog.get(), "scrollingBarLineWidthSpinBox")->setValue(5);
    control<QSpinBox>(pDialog.get(), "scrollingBeatLineWidthSpinBox")->setValue(2);
    EXPECT_EQ(5, factorySettings().barWidth.px());
    EXPECT_EQ(2, factorySettings().beatWidth.px());
    const LineSettings stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    EXPECT_EQ(5, stored.barWidth.px());
    EXPECT_EQ(2, stored.beatWidth.px());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, CustomColourAndBackToSkinColour) {
    auto pDialog = openPage();
    pickColor(pDialog.get(), "scrollingBeatLineColorButton", QColor(0xff, 0x80, 0x00));
    ASSERT_TRUE(factorySettings().beatColor.has_value());
    EXPECT_EQ(QColor(0xff, 0x80, 0x00), *factorySettings().beatColor);
    EXPECT_FALSE(factorySettings().barColor.has_value());
    auto stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    ASSERT_TRUE(stored.beatColor.has_value());
    EXPECT_EQ(QColor(0xff, 0x80, 0x00), *stored.beatColor);
    auto* pSkinButton = control<QPushButton>(pDialog.get(), "scrollingBeatLineSkinColorButton");
    EXPECT_TRUE(pSkinButton->isEnabled());
    EXPECT_NE(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingBeatLineColorButton")->text());

    pSkinButton->click();
    EXPECT_FALSE(factorySettings().beatColor.has_value());
    EXPECT_FALSE(mixxx::scrollinglinelayout::loadLineSettings(config()).beatColor.has_value());
    EXPECT_FALSE(pSkinButton->isEnabled());
    EXPECT_EQ(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingBeatLineColorButton")->text());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, ResetToDefaultsResetsWidthsAndColours) {
    storeNonDefaults(config());
    auto pDialog = openPage();
    pDialog->slotResetToDefaults();
    EXPECT_EQ(2, control<QSpinBox>(pDialog.get(), "scrollingBarLineWidthSpinBox")->value());
    EXPECT_EQ(1, control<QSpinBox>(pDialog.get(), "scrollingBeatLineWidthSpinBox")->value());
    EXPECT_EQ(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingBarLineColorButton")->text());
    EXPECT_EQ(2, factorySettings().barWidth.px());
    EXPECT_FALSE(factorySettings().barColor.has_value());
    const LineSettings stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    EXPECT_EQ(2, stored.barWidth.px());
    EXPECT_EQ(1, stored.beatWidth.px());
    EXPECT_FALSE(stored.barColor.has_value());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, GroupShowsStepDefaults) {
    auto pDialog = openPage();
    auto* pSteps = control<QCheckBox>(pDialog.get(), "scrollingStepLinesEnabledCheckBox");
    auto* pSpacing = control<QSpinBox>(pDialog.get(), "scrollingStepLineMinSpacingSpinBox");
    auto* pOpacity = control<QSpinBox>(pDialog.get(), "scrollingStepLineOpacitySpinBox");
    auto* pWidth = control<QSpinBox>(pDialog.get(), "scrollingStepLineWidthSpinBox");
    auto* pColor = control<QPushButton>(pDialog.get(), "scrollingStepLineColorButton");
    ASSERT_TRUE(pSteps && pSpacing && pOpacity && pWidth && pColor);
    EXPECT_FALSE(pSteps->isChecked());
    EXPECT_EQ(8, pSpacing->value());
    EXPECT_EQ(4, pSpacing->minimum());
    EXPECT_EQ(30, pSpacing->maximum());
    EXPECT_EQ(45, pOpacity->value());
    EXPECT_EQ(45, control<QSlider>(pDialog.get(), "scrollingStepLineOpacitySlider")->value());
    EXPECT_EQ(1, pWidth->value());
    EXPECT_EQ(QStringLiteral("Skin colour"), pColor->text());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, StepEditsApplyLiveAndPersist) {
    auto pDialog = openPage();
    control<QCheckBox>(pDialog.get(), "scrollingStepLinesEnabledCheckBox")->setChecked(true);
    EXPECT_TRUE(factorySettings().stepsEnabled);
    control<QSpinBox>(pDialog.get(), "scrollingStepLineMinSpacingSpinBox")->setValue(12);
    control<QSlider>(pDialog.get(), "scrollingStepLineOpacitySlider")->setValue(60);
    control<QSpinBox>(pDialog.get(), "scrollingStepLineWidthSpinBox")->setValue(3);
    pickColor(pDialog.get(), "scrollingStepLineColorButton", QColor(0x00, 0x80, 0xff));
    EXPECT_EQ(12, factorySettings().stepMinSpacing.px());
    EXPECT_EQ(60, factorySettings().stepOpacity.percent());
    EXPECT_EQ(3, factorySettings().stepWidth.px());
    ASSERT_TRUE(factorySettings().stepColor.has_value());
    EXPECT_EQ(QColor(0x00, 0x80, 0xff), *factorySettings().stepColor);
    // The other colours are untouched.
    EXPECT_FALSE(factorySettings().beatColor.has_value());

    const LineSettings stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    EXPECT_TRUE(stored.stepsEnabled);
    EXPECT_EQ(12, stored.stepMinSpacing.px());
    EXPECT_EQ(60, stored.stepOpacity.percent());
    EXPECT_EQ(3, stored.stepWidth.px());
    ASSERT_TRUE(stored.stepColor.has_value());
    EXPECT_EQ(QColor(0x00, 0x80, 0xff), *stored.stepColor);
}

TEST_F(DlgPrefWaveformScrollingLinesTest, ResetToDefaultsResetsStepSettings) {
    LineSettings settings;
    settings.stepsEnabled = true;
    settings.stepMinSpacing = mixxx::scrollinglinelayout::StepSpacingPx(20);
    settings.stepOpacity = OpacityPercent(10);
    settings.stepWidth = LineWidthPx(5);
    settings.stepColor = QColor(0x44, 0x55, 0x66);
    mixxx::scrollinglinelayout::saveLineSettings(config(), settings);
    WaveformWidgetFactory::instance()->setConfig(config());
    auto pDialog = openPage();
    EXPECT_TRUE(control<QCheckBox>(pDialog.get(), "scrollingStepLinesEnabledCheckBox")
                        ->isChecked());
    EXPECT_EQ(20, control<QSpinBox>(pDialog.get(), "scrollingStepLineMinSpacingSpinBox")->value());

    pDialog->slotResetToDefaults();
    EXPECT_FALSE(control<QCheckBox>(pDialog.get(), "scrollingStepLinesEnabledCheckBox")
                         ->isChecked());
    EXPECT_EQ(8, control<QSpinBox>(pDialog.get(), "scrollingStepLineMinSpacingSpinBox")->value());
    EXPECT_EQ(45, control<QSpinBox>(pDialog.get(), "scrollingStepLineOpacitySpinBox")->value());
    EXPECT_EQ(1, control<QSpinBox>(pDialog.get(), "scrollingStepLineWidthSpinBox")->value());
    EXPECT_EQ(QStringLiteral("Skin colour"),
            control<QPushButton>(pDialog.get(), "scrollingStepLineColorButton")->text());
    EXPECT_FALSE(factorySettings().stepsEnabled);
    const LineSettings stored = mixxx::scrollinglinelayout::loadLineSettings(config());
    EXPECT_FALSE(stored.stepsEnabled);
    EXPECT_EQ(8, stored.stepMinSpacing.px());
    EXPECT_FALSE(stored.stepColor.has_value());
}

TEST_F(DlgPrefWaveformScrollingLinesTest, StepRowsAreGreyedOutWhileStepLinesAreOff) {
    auto pDialog = openPage();
    auto* pSteps = control<QCheckBox>(pDialog.get(), "scrollingStepLinesEnabledCheckBox");
    const auto stepControls = [&pDialog]() {
        return std::vector<QWidget*>{
                control<QSpinBox>(pDialog.get(), "scrollingStepLineMinSpacingSpinBox"),
                control<QSlider>(pDialog.get(), "scrollingStepLineOpacitySlider"),
                control<QSpinBox>(pDialog.get(), "scrollingStepLineOpacitySpinBox"),
                control<QSpinBox>(pDialog.get(), "scrollingStepLineWidthSpinBox"),
                control<QPushButton>(pDialog.get(), "scrollingStepLineColorButton")};
    };
    auto* pSkinColor = control<QPushButton>(pDialog.get(), "scrollingStepLineSkinColorButton");
    ASSERT_TRUE(pSteps && pSkinColor);
    // Off by default.
    for (QWidget* pControl : stepControls()) {
        EXPECT_FALSE(pControl->isEnabled()) << pControl->objectName().toStdString();
    }
    pSteps->setChecked(true);
    for (QWidget* pControl : stepControls()) {
        EXPECT_TRUE(pControl->isEnabled()) << pControl->objectName().toStdString();
    }
    // "Skin colour" still only with a custom colour.
    EXPECT_FALSE(pSkinColor->isEnabled());
    pickColor(pDialog.get(), "scrollingStepLineColorButton", QColor(0x00, 0x80, 0xff));
    EXPECT_TRUE(pSkinColor->isEnabled());
    pSteps->setChecked(false);
    EXPECT_FALSE(pSkinColor->isEnabled());
    for (QWidget* pControl : stepControls()) {
        EXPECT_FALSE(pControl->isEnabled()) << pControl->objectName().toStdString();
    }
    // The bar and beat rows are not affected.
    EXPECT_TRUE(control<QSpinBox>(pDialog.get(), "scrollingBeatLineWidthSpinBox")->isEnabled());
}
