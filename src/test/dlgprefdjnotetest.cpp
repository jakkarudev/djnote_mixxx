#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QListWidget>
#include <set>
#include <memory>

#include "library/keymatch/keymatcher.h"
#include "library/keymatch/keymatchsettings.h"
#include "mixer/playerinfo.h"
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

TEST_F(DlgPrefDJNoteTest, SwitchingTheReadoutAppliesLive) {
    // Like the Waveforms page: no Apply needed.
    auto pPage = openPage();
    enabledCheckBox(pPage.get())->setChecked(false);
    EXPECT_FALSE(mixxx::knobreadout::loadSettings(config()).enabled);
    enabledCheckBox(pPage.get())->setChecked(true);
    EXPECT_TRUE(mixxx::knobreadout::loadSettings(config()).enabled);
}

TEST_F(DlgPrefDJNoteTest, CancelKeepsTheLiveSetting) {
    auto pPage = openPage();
    enabledCheckBox(pPage.get())->setChecked(false);
    pPage->slotCancel();
    EXPECT_FALSE(enabledCheckBox(pPage.get())->isChecked());
    EXPECT_FALSE(mixxx::knobreadout::loadSettings(config()).enabled);
}

TEST_F(DlgPrefDJNoteTest, OpeningThePageStoresNothing) {
    auto pPage = openPage();
    EXPECT_FALSE(config()->exists(ConfigKey("[DJNote]", "knobReadoutEnabled")));
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresTheDefault) {
    storeEnabled(false);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_TRUE(enabledCheckBox(pPage.get())->isChecked());
    pPage->slotApply();
    EXPECT_TRUE(mixxx::knobreadout::loadSettings(config()).enabled);
}

// Style (ticket #70).
namespace {

using mixxx::knobreadout::Style;

QComboBox* styleComboBox(DlgPrefDJNote* pPage) {
    auto* pComboBox = pPage->findChild<QComboBox*>(
            QStringLiteral("knobReadoutStyleComboBox"));
    EXPECT_NE(nullptr, pComboBox);
    return pComboBox;
}

} // namespace

TEST_F(DlgPrefDJNoteTest, StyleOffersBothStylesUnderTheKnobByDefault) {
    auto pPage = openPage();
    auto* pStyle = styleComboBox(pPage.get());
    ASSERT_NE(nullptr, pStyle);
    ASSERT_EQ(2, pStyle->count());
    EXPECT_EQ(QStringLiteral("Under the knob"), pStyle->itemText(0));
    EXPECT_EQ(QStringLiteral("On the knob face"), pStyle->itemText(1));
    EXPECT_EQ(QStringLiteral("Under the knob"), pStyle->currentText());
}

TEST_F(DlgPrefDJNoteTest, StyleShowsAndStoresTheSetting) {
    Settings stored;
    stored.style = Style::KnobFace;
    mixxx::knobreadout::saveSettings(config(), stored);
    auto pPage = openPage();
    auto* pStyle = styleComboBox(pPage.get());
    ASSERT_NE(nullptr, pStyle);
    EXPECT_EQ(QStringLiteral("On the knob face"), pStyle->currentText());

    pStyle->setCurrentIndex(pStyle->findText(QStringLiteral("Under the knob")));
    pPage->slotApply();
    EXPECT_EQ(Style::UnderKnob, mixxx::knobreadout::loadSettings(config()).style);
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresUnderTheKnob) {
    Settings stored;
    stored.style = Style::KnobFace;
    mixxx::knobreadout::saveSettings(config(), stored);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_EQ(QStringLiteral("Under the knob"), styleComboBox(pPage.get())->currentText());
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

TEST_F(DlgPrefDJNoteTest, ModeAndStyleApplyLive) {
    auto pPage = openPage();
    auto* pMode = modeComboBox(pPage.get());
    pMode->setCurrentIndex(pMode->findText(QStringLiteral("Precise")));
    EXPECT_EQ(mixxx::knobreadout::Mode::Precise,
            mixxx::knobreadout::loadSettings(config()).mode);
    auto* pStyle = styleComboBox(pPage.get());
    pStyle->setCurrentIndex(pStyle->findText(QStringLiteral("On the knob face")));
    EXPECT_EQ(Style::KnobFace, mixxx::knobreadout::loadSettings(config()).style);
}

TEST_F(DlgPrefDJNoteTest, ResetAppliesTheDefaultsLive) {
    Settings stored;
    stored.enabled = false;
    stored.mode = mixxx::knobreadout::Mode::Precise;
    stored.style = Style::KnobFace;
    mixxx::knobreadout::saveSettings(config(), stored);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    const Settings settings = mixxx::knobreadout::loadSettings(config());
    EXPECT_TRUE(settings.enabled);
    EXPECT_EQ(mixxx::knobreadout::Mode::Simple, settings.mode);
    EXPECT_EQ(Style::UnderKnob, settings.style);
}

TEST_F(DlgPrefDJNoteTest, StyleIsGreyedOutWhileTheReadoutIsOff) {
    auto pPage = openPage();
    EXPECT_TRUE(styleComboBox(pPage.get())->isEnabled());
    enabledCheckBox(pPage.get())->setChecked(false);
    EXPECT_FALSE(styleComboBox(pPage.get())->isEnabled());
    enabledCheckBox(pPage.get())->setChecked(true);
    EXPECT_TRUE(styleComboBox(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, StoredOffShowsModeAndStyleGreyedOut) {
    storeEnabled(false);
    auto pPage = openPage();
    EXPECT_FALSE(modeComboBox(pPage.get())->isEnabled());
    EXPECT_FALSE(styleComboBox(pPage.get())->isEnabled());
}

// Key matching (ticket #96): a group with the Highlight combo box.
namespace {

using mixxx::keymatch::Highlight;

QComboBox* highlightComboBox(DlgPrefDJNote* pPage) {
    auto* pComboBox = pPage->findChild<QComboBox*>(
            QStringLiteral("keyMatchingHighlightComboBox"));
    EXPECT_NE(nullptr, pComboBox);
    return pComboBox;
}

QCheckBox* boldCheckBox(DlgPrefDJNote* pPage) {
    auto* pCheckBox = pPage->findChild<QCheckBox*>(
            QStringLiteral("keyMatchingBoldCheckBox"));
    EXPECT_NE(nullptr, pCheckBox);
    return pCheckBox;
}

void storeHighlight(const UserSettingsPointer& pConfig, Highlight highlight) {
    mixxx::keymatch::Settings settings;
    settings.highlight = highlight;
    mixxx::keymatch::saveSettings(pConfig, settings);
}

} // namespace

TEST_F(DlgPrefDJNoteTest, KeyMatchingGroupOffersTheHighlightChoices) {
    auto pPage = openPage();
    auto* pGroup = pPage->findChild<QGroupBox*>(QStringLiteral("keyMatchingGroupBox"));
    ASSERT_NE(nullptr, pGroup);
    EXPECT_EQ(QStringLiteral("Key matching"), pGroup->title());
    auto* pHighlight = highlightComboBox(pPage.get());
    ASSERT_NE(nullptr, pHighlight);
    ASSERT_EQ(3, pHighlight->count());
    EXPECT_EQ(QStringLiteral("Off"), pHighlight->itemText(0));
    EXPECT_EQ(QStringLiteral("Strict only"), pHighlight->itemText(1));
    EXPECT_EQ(QStringLiteral("Strict and fuzzy"), pHighlight->itemText(2));
}

TEST_F(DlgPrefDJNoteTest, HighlightIsStrictAndFuzzyByDefault) {
    auto pPage = openPage();
    EXPECT_EQ(QStringLiteral("Strict and fuzzy"), highlightComboBox(pPage.get())->currentText());
}

TEST_F(DlgPrefDJNoteTest, HighlightShowsTheStoredSetting) {
    storeHighlight(config(), Highlight::StrictOnly);
    auto pPage = openPage();
    EXPECT_EQ(QStringLiteral("Strict only"), highlightComboBox(pPage.get())->currentText());
}

TEST_F(DlgPrefDJNoteTest, HighlightAppliesLive) {
    auto pPage = openPage();
    auto* pHighlight = highlightComboBox(pPage.get());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
    EXPECT_EQ(Highlight::Off, mixxx::keymatch::loadSettings(config()).highlight);
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Strict only")));
    EXPECT_EQ(Highlight::StrictOnly, mixxx::keymatch::loadSettings(config()).highlight);
}

TEST_F(DlgPrefDJNoteTest, HighlightTellsTheKeyMatcher) {
    PlayerInfo::create();
    {
        auto pPage = openPage();
        mixxx::keymatch::KeyMatcher matcher(config());
        auto* pHighlight = highlightComboBox(pPage.get());
        pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
        EXPECT_EQ(Highlight::Off, matcher.settings().highlight);
    }
    PlayerInfo::destroy();
}

TEST_F(DlgPrefDJNoteTest, CancelKeepsTheLiveHighlight) {
    auto pPage = openPage();
    auto* pHighlight = highlightComboBox(pPage.get());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
    pPage->slotCancel();
    EXPECT_EQ(QStringLiteral("Off"), pHighlight->currentText());
    EXPECT_EQ(Highlight::Off, mixxx::keymatch::loadSettings(config()).highlight);
}

TEST_F(DlgPrefDJNoteTest, OpeningThePageStoresNoKeyMatchingSetting) {
    auto pPage = openPage();
    EXPECT_FALSE(config()->exists(ConfigKey("[DJNote]", "KeyMatchingHighlight")));
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresStrictAndFuzzy) {
    storeHighlight(config(), Highlight::Off);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_EQ(QStringLiteral("Strict and fuzzy"), highlightComboBox(pPage.get())->currentText());
    EXPECT_EQ(Highlight::StrictAndFuzzy, mixxx::keymatch::loadSettings(config()).highlight);
}

// Colour these columns (ticket #99): a checklist with Whole row first.
namespace {

using mixxx::keymatch::ColumnIdentity;

QListWidget* columnsList(DlgPrefDJNote* pPage) {
    auto* pList = pPage->findChild<QListWidget*>(QStringLiteral("keyMatchingColumnsList"));
    EXPECT_NE(nullptr, pList);
    return pList;
}

QListWidgetItem* columnItem(DlgPrefDJNote* pPage, const QString& text) {
    auto* pList = columnsList(pPage);
    const auto items = pList->findItems(text, Qt::MatchExactly);
    EXPECT_EQ(1, items.size()) << text.toStdString();
    return items.isEmpty() ? nullptr : items.first();
}

bool isTicked(QListWidgetItem* pItem) {
    return pItem->checkState() == Qt::Checked;
}

bool isGreyedOut(QListWidgetItem* pItem) {
    return !pItem->flags().testFlag(Qt::ItemIsEnabled);
}

} // namespace

TEST_F(DlgPrefDJNoteTest, ColumnsChecklistStartsWithWholeRowThenTextColumns) {
    auto pPage = openPage();
    auto* pList = columnsList(pPage.get());
    ASSERT_GT(pList->count(), 4);
    EXPECT_EQ(QStringLiteral("Whole row"), pList->item(0)->text());
    EXPECT_EQ(QStringLiteral("Key"), pList->item(1)->text());
    for (const char* text : {"Title", "Artist", "BPM"}) {
        EXPECT_EQ(1, pList->findItems(QString::fromLatin1(text), Qt::MatchExactly).size()) << text;
    }
    for (const char* text : {"Cover", "Color", "Rating", "Played", "Preview"}) {
        EXPECT_TRUE(pList->findItems(QString::fromLatin1(text), Qt::MatchExactly).isEmpty())
                << text;
    }
}

TEST_F(DlgPrefDJNoteTest, OnlyKeyIsTickedByDefault) {
    auto pPage = openPage();
    auto* pList = columnsList(pPage.get());
    for (int i = 0; i < pList->count(); ++i) {
        EXPECT_EQ(pList->item(i)->text() == QStringLiteral("Key"), isTicked(pList->item(i)))
                << pList->item(i)->text().toStdString();
    }
}

TEST_F(DlgPrefDJNoteTest, ColumnsShowTheStoredSetting) {
    mixxx::keymatch::Settings settings;
    settings.columns = {ColumnIdentity::Title};
    settings.wholeRow = true;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    EXPECT_TRUE(isTicked(columnItem(pPage.get(), QStringLiteral("Whole row"))));
    EXPECT_TRUE(isTicked(columnItem(pPage.get(), QStringLiteral("Title"))));
    EXPECT_FALSE(isTicked(columnItem(pPage.get(), QStringLiteral("Key"))));
}

TEST_F(DlgPrefDJNoteTest, TickingColumnsAppliesLive) {
    auto pPage = openPage();
    columnItem(pPage.get(), QStringLiteral("Title"))->setCheckState(Qt::Checked);
    columnItem(pPage.get(), QStringLiteral("Key"))->setCheckState(Qt::Unchecked);
    auto settings = mixxx::keymatch::loadSettings(config());
    EXPECT_EQ((std::set<ColumnIdentity>{ColumnIdentity::Title}), settings.columns);
    EXPECT_FALSE(settings.wholeRow);
    columnItem(pPage.get(), QStringLiteral("Whole row"))->setCheckState(Qt::Checked);
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).wholeRow);
}

TEST_F(DlgPrefDJNoteTest, WholeRowGreysOutTheOtherColumns) {
    auto pPage = openPage();
    EXPECT_FALSE(isGreyedOut(columnItem(pPage.get(), QStringLiteral("Title"))));
    columnItem(pPage.get(), QStringLiteral("Whole row"))->setCheckState(Qt::Checked);
    EXPECT_FALSE(isGreyedOut(columnItem(pPage.get(), QStringLiteral("Whole row"))));
    EXPECT_TRUE(isGreyedOut(columnItem(pPage.get(), QStringLiteral("Title"))));
    EXPECT_TRUE(isGreyedOut(columnItem(pPage.get(), QStringLiteral("Key"))));
    columnItem(pPage.get(), QStringLiteral("Whole row"))->setCheckState(Qt::Unchecked);
    EXPECT_FALSE(isGreyedOut(columnItem(pPage.get(), QStringLiteral("Title"))));
}

TEST_F(DlgPrefDJNoteTest, ChecklistIsGreyedOutWhileHighlightIsOff) {
    auto pPage = openPage();
    EXPECT_TRUE(columnsList(pPage.get())->isEnabled());
    auto* pHighlight = highlightComboBox(pPage.get());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
    EXPECT_FALSE(columnsList(pPage.get())->isEnabled());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Strict only")));
    EXPECT_TRUE(columnsList(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, ChecklistIsGreyedOutWhenStoredHighlightIsOff) {
    storeHighlight(config(), Highlight::Off);
    auto pPage = openPage();
    EXPECT_FALSE(columnsList(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresKeyOnly) {
    mixxx::keymatch::Settings settings;
    settings.columns = {ColumnIdentity::Title};
    settings.wholeRow = true;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_TRUE(isTicked(columnItem(pPage.get(), QStringLiteral("Key"))));
    EXPECT_FALSE(isTicked(columnItem(pPage.get(), QStringLiteral("Title"))));
    EXPECT_FALSE(isTicked(columnItem(pPage.get(), QStringLiteral("Whole row"))));
    const auto stored = mixxx::keymatch::loadSettings(config());
    EXPECT_EQ((std::set<ColumnIdentity>{ColumnIdentity::Key}), stored.columns);
    EXPECT_FALSE(stored.wholeRow);
}

TEST_F(DlgPrefDJNoteTest, ColumnSettingsRoundTripIncludingNoColumns) {
    mixxx::keymatch::Settings settings;
    settings.columns = {ColumnIdentity::Artist, ColumnIdentity::Bpm};
    settings.wholeRow = true;
    mixxx::keymatch::saveSettings(config(), settings);
    auto loaded = mixxx::keymatch::loadSettings(config());
    EXPECT_EQ(settings.columns, loaded.columns);
    EXPECT_TRUE(loaded.wholeRow);
    settings.columns.clear();
    mixxx::keymatch::saveSettings(config(), settings);
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).columns.empty());
}

TEST_F(DlgPrefDJNoteTest, KeyMatchingSettingsRoundTripAndFallBack) {
    EXPECT_EQ(Highlight::StrictAndFuzzy, mixxx::keymatch::loadSettings(config()).highlight);
    for (Highlight highlight :
            {Highlight::Off, Highlight::StrictOnly, Highlight::StrictAndFuzzy}) {
        storeHighlight(config(), highlight);
        EXPECT_EQ(highlight, mixxx::keymatch::loadSettings(config()).highlight);
    }
    config()->setValue(ConfigKey("[DJNote]", "KeyMatchingHighlight"), QStringLiteral("bogus"));
    EXPECT_EQ(Highlight::StrictAndFuzzy, mixxx::keymatch::loadSettings(config()).highlight);
}

// Key matching (ticket #97): the shifted key checkbox.
namespace {

QCheckBox* shiftedKeyCheckBox(DlgPrefDJNote* pPage) {
    auto* pCheckBox = pPage->findChild<QCheckBox*>(
            QStringLiteral("keyMatchingShiftedKeyCheckBox"));
    EXPECT_NE(nullptr, pCheckBox);
    return pCheckBox;
}

} // namespace

TEST_F(DlgPrefDJNoteTest, ShiftedKeyCheckBoxIsOnByDefault) {
    auto pPage = openPage();
    auto* pCheckBox = shiftedKeyCheckBox(pPage.get());
    EXPECT_EQ(QStringLiteral("Also match the shifted key of the reference deck"),
            pCheckBox->text());
    EXPECT_TRUE(pCheckBox->isChecked());
}

TEST_F(DlgPrefDJNoteTest, ShiftedKeyCheckBoxShowsTheStoredSetting) {
    mixxx::keymatch::Settings settings;
    settings.matchShiftedKey = false;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    EXPECT_FALSE(shiftedKeyCheckBox(pPage.get())->isChecked());
}

TEST_F(DlgPrefDJNoteTest, ShiftedKeyCheckBoxAppliesLiveAndKeepsHighlight) {
    storeHighlight(config(), Highlight::StrictOnly);
    auto pPage = openPage();
    shiftedKeyCheckBox(pPage.get())->setChecked(false);
    EXPECT_FALSE(mixxx::keymatch::loadSettings(config()).matchShiftedKey);
    EXPECT_EQ(Highlight::StrictOnly, mixxx::keymatch::loadSettings(config()).highlight);
}

TEST_F(DlgPrefDJNoteTest, ShiftedKeyCheckBoxTellsTheKeyMatcher) {
    PlayerInfo::create();
    {
        auto pPage = openPage();
        mixxx::keymatch::KeyMatcher matcher(config());
        shiftedKeyCheckBox(pPage.get())->setChecked(false);
        EXPECT_FALSE(matcher.settings().matchShiftedKey);
    }
    PlayerInfo::destroy();
}


// Bold matched text (ticket #100).
TEST_F(DlgPrefDJNoteTest, BoldMatchedTextIsOnByDefault) {
    auto pPage = openPage();
    auto* pBold = boldCheckBox(pPage.get());
    ASSERT_NE(nullptr, pBold);
    EXPECT_EQ(QStringLiteral("Bold matched text"), pBold->text());
    EXPECT_TRUE(pBold->isChecked());
    EXPECT_TRUE(pBold->isEnabled());
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).boldMatchedText);
}

TEST_F(DlgPrefDJNoteTest, BoldMatchedTextShowsTheStoredSetting) {
    mixxx::keymatch::Settings settings;
    settings.boldMatchedText = false;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    EXPECT_FALSE(boldCheckBox(pPage.get())->isChecked());
}

TEST_F(DlgPrefDJNoteTest, BoldMatchedTextAppliesLiveAndTellsTheMatcher) {
    PlayerInfo::create();
    {
        auto pPage = openPage();
        mixxx::keymatch::KeyMatcher matcher(config());
        boldCheckBox(pPage.get())->setChecked(false);
        EXPECT_FALSE(mixxx::keymatch::loadSettings(config()).boldMatchedText);
        EXPECT_FALSE(matcher.settings().boldMatchedText);
        boldCheckBox(pPage.get())->setChecked(true);
        EXPECT_TRUE(matcher.settings().boldMatchedText);
    }
    PlayerInfo::destroy();
}

TEST_F(DlgPrefDJNoteTest, BoldMatchedTextIsGreyedOutWhenHighlightIsOff) {
    auto pPage = openPage();
    auto* pHighlight = highlightComboBox(pPage.get());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
    EXPECT_FALSE(boldCheckBox(pPage.get())->isEnabled());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Strict only")));
    EXPECT_TRUE(boldCheckBox(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, BoldMatchedTextStartsGreyedOutWhenStoredHighlightIsOff) {
    storeHighlight(config(), Highlight::Off);
    auto pPage = openPage();
    EXPECT_FALSE(boldCheckBox(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresBoldMatchedText) {
    mixxx::keymatch::Settings settings;
    settings.highlight = Highlight::Off;
    settings.boldMatchedText = false;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_TRUE(boldCheckBox(pPage.get())->isChecked());
    EXPECT_TRUE(boldCheckBox(pPage.get())->isEnabled());
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).boldMatchedText);
}

TEST_F(DlgPrefDJNoteTest, BoldMatchedTextSettingRoundTripsAndFallsBack) {
    mixxx::keymatch::Settings settings;
    settings.boldMatchedText = false;
    mixxx::keymatch::saveSettings(config(), settings);
    EXPECT_FALSE(mixxx::keymatch::loadSettings(config()).boldMatchedText);
    config()->setValue(ConfigKey("[DJNote]", "KeyMatchingBold"), QStringLiteral("bogus"));
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).boldMatchedText);
}

TEST_F(DlgPrefDJNoteTest, ShiftedKeyCheckBoxIsGreyedOutWhileHighlightIsOff) {
    auto pPage = openPage();
    auto* pHighlight = highlightComboBox(pPage.get());
    EXPECT_TRUE(shiftedKeyCheckBox(pPage.get())->isEnabled());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Off")));
    EXPECT_FALSE(shiftedKeyCheckBox(pPage.get())->isEnabled());
    pHighlight->setCurrentIndex(pHighlight->findText(QStringLiteral("Strict only")));
    EXPECT_TRUE(shiftedKeyCheckBox(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, StoredOffShowsTheShiftedKeyCheckBoxGreyedOut) {
    storeHighlight(config(), Highlight::Off);
    auto pPage = openPage();
    EXPECT_FALSE(shiftedKeyCheckBox(pPage.get())->isEnabled());
}

TEST_F(DlgPrefDJNoteTest, ResetRestoresTheShiftedKeyCheckBox) {
    mixxx::keymatch::Settings settings;
    settings.matchShiftedKey = false;
    settings.highlight = Highlight::Off;
    mixxx::keymatch::saveSettings(config(), settings);
    auto pPage = openPage();
    pPage->slotResetToDefaults();
    EXPECT_TRUE(shiftedKeyCheckBox(pPage.get())->isChecked());
    EXPECT_TRUE(shiftedKeyCheckBox(pPage.get())->isEnabled());
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).matchShiftedKey);
}

TEST_F(DlgPrefDJNoteTest, ShiftedKeySettingRoundTrips) {
    EXPECT_TRUE(mixxx::keymatch::loadSettings(config()).matchShiftedKey);
    mixxx::keymatch::Settings settings;
    settings.matchShiftedKey = false;
    mixxx::keymatch::saveSettings(config(), settings);
    EXPECT_FALSE(mixxx::keymatch::loadSettings(config()).matchShiftedKey);
}

