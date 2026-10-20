#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <memory>

#include "preferences/dialog/dlgprefdjnote.h"
#include "test/mixxxtest.h"
#include "widget/knobreadoutsettings.h"

// The DJNote preferences page (ticket #67), driven through its controls.
namespace {

using mixxx::knobreadout::Settings;

class DlgPrefDJNoteTest : public MixxxTest {
  protected:
    std::unique_ptr<DlgPrefDJNote> openPage() {
        auto pPage = std::make_unique<DlgPrefDJNote>(nullptr, config());
        pPage->slotUpdate();
        return pPage;
    }

    static QCheckBox* enabledCheckBox(DlgPrefDJNote* pPage) {
        auto* pCheckBox = pPage->findChild<QCheckBox*>(
                QStringLiteral("knobReadoutEnabledCheckBox"));
        EXPECT_NE(nullptr, pCheckBox);
        return pCheckBox;
    }

    void storeEnabled(bool enabled) {
        Settings settings;
        settings.enabled = enabled;
        mixxx::knobreadout::saveSettings(config(), settings);
    }
};

} // namespace

TEST_F(DlgPrefDJNoteTest, PageShowsTheDefault) {
    auto pPage = openPage();
    auto* pGroup = pPage->findChild<QGroupBox*>(QStringLiteral("knobReadoutGroupBox"));
    ASSERT_NE(nullptr, pGroup);
    EXPECT_EQ(QStringLiteral("Knob adjustment readout"), pGroup->title());
    auto* pEnabled = enabledCheckBox(pPage.get());
    ASSERT_NE(nullptr, pEnabled);
    EXPECT_EQ(QStringLiteral("Show knob adjustment readout"), pEnabled->text());
    EXPECT_TRUE(pEnabled->isChecked());
}

TEST_F(DlgPrefDJNoteTest, PageShowsTheStoredSetting) {
    storeEnabled(false);
    auto pPage = openPage();
    EXPECT_FALSE(enabledCheckBox(pPage.get())->isChecked());
}

TEST_F(DlgPrefDJNoteTest, ApplyStoresTheSetting) {
    auto pPage = openPage();
    enabledCheckBox(pPage.get())->setChecked(false);
    pPage->slotApply();
    EXPECT_FALSE(mixxx::knobreadout::loadSettings(config()).enabled);

    enabledCheckBox(pPage.get())->setChecked(true);
    pPage->slotApply();
    EXPECT_TRUE(mixxx::knobreadout::loadSettings(config()).enabled);
}

TEST_F(DlgPrefDJNoteTest, CancelShowsTheStoredSettingAgain) {
    auto pPage = openPage();
    enabledCheckBox(pPage.get())->setChecked(false);
    pPage->slotCancel();
    EXPECT_TRUE(enabledCheckBox(pPage.get())->isChecked());
    EXPECT_TRUE(mixxx::knobreadout::loadSettings(config()).enabled);
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresTheDefault) {
    storeEnabled(false);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_TRUE(enabledCheckBox(pPage.get())->isChecked());
    pPage->slotApply();
    EXPECT_TRUE(mixxx::knobreadout::loadSettings(config()).enabled);
}

// Mode (ticket #68): a Simple / Precise combo box, default Simple.
namespace {

QComboBox* modeComboBox(DlgPrefDJNote* pPage) {
    auto* pComboBox = pPage->findChild<QComboBox*>(
            QStringLiteral("knobReadoutModeComboBox"));
    EXPECT_NE(nullptr, pComboBox);
    return pComboBox;
}

} // namespace

TEST_F(DlgPrefDJNoteTest, ModeShowsSimpleByDefault) {
    auto pPage = openPage();
    auto* pMode = modeComboBox(pPage.get());
    ASSERT_NE(nullptr, pMode);
    ASSERT_EQ(2, pMode->count());
    EXPECT_EQ(QStringLiteral("Simple"), pMode->itemText(0));
    EXPECT_EQ(QStringLiteral("Precise"), pMode->itemText(1));
    EXPECT_EQ(QStringLiteral("Simple"), pMode->currentText());
}

TEST_F(DlgPrefDJNoteTest, ModeShowsTheStoredSetting) {
    Settings settings;
    settings.mode = mixxx::knobreadout::Mode::Precise;
    mixxx::knobreadout::saveSettings(config(), settings);
    auto pPage = openPage();
    EXPECT_EQ(QStringLiteral("Precise"), modeComboBox(pPage.get())->currentText());
}

TEST_F(DlgPrefDJNoteTest, ApplyStoresTheMode) {
    auto pPage = openPage();
    modeComboBox(pPage.get())->setCurrentIndex(1);
    pPage->slotApply();
    EXPECT_EQ(mixxx::knobreadout::Mode::Precise,
            mixxx::knobreadout::loadSettings(config()).mode);
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresSimpleMode) {
    Settings settings;
    settings.mode = mixxx::knobreadout::Mode::Precise;
    mixxx::knobreadout::saveSettings(config(), settings);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_EQ(QStringLiteral("Simple"), modeComboBox(pPage.get())->currentText());
    pPage->slotApply();
    EXPECT_EQ(mixxx::knobreadout::Mode::Simple,
            mixxx::knobreadout::loadSettings(config()).mode);
}

TEST_F(DlgPrefDJNoteTest, ModeIsGreyedOutWhileTheReadoutIsOff) {
    auto pPage = openPage();
    EXPECT_TRUE(modeComboBox(pPage.get())->isEnabled());
    enabledCheckBox(pPage.get())->setChecked(false);
    EXPECT_FALSE(modeComboBox(pPage.get())->isEnabled());
    enabledCheckBox(pPage.get())->setChecked(true);
    EXPECT_TRUE(modeComboBox(pPage.get())->isEnabled());
}
