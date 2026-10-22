#include "preferences/dialog/dlgprefdjnote.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "library/keymatch/keymatcher.h"

#include "moc_dlgprefdjnote.cpp"

namespace {

// The Whole row item's data, apart from every ColumnIdentity.
constexpr int kWholeRowItem = -1;

} // namespace

DlgPrefDJNote::DlgPrefDJNote(QWidget* pParent, UserSettingsPointer pConfig)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pKnobReadoutEnabled(nullptr),
          m_pKnobReadoutModeLabel(nullptr),
          m_pKnobReadoutMode(nullptr),
          m_pKnobReadoutStyleLabel(nullptr),
          m_pKnobReadoutStyle(nullptr),
          m_pKeyMatchingHighlight(nullptr),
          m_pKeyMatchingShiftedKey(nullptr),
          m_pKeyMatchingColumns(nullptr),
          m_pKeyMatchingBold(nullptr),
          m_keyMatchingColorButtons{},
          m_keyMatchingColorLabels{},
          m_pKeyMatchingResetColors(nullptr),
          m_showingKeyMatchingSettings(false) {
    auto* pPageLayout = new QVBoxLayout(this);
    pPageLayout->setContentsMargins(0, 0, 0, 0);
    createKnobReadoutGroup();
    createKeyMatchingGroup();
    pPageLayout->addStretch(1);
    setScrollSafeGuardForAllInputWidgets(this);
}

void DlgPrefDJNote::createKnobReadoutGroup() {
    auto* pGroup = new QGroupBox(tr("Knob adjustment readout"), this);
    pGroup->setObjectName(QStringLiteral("knobReadoutGroupBox"));
    // Two columns (label, control), so later settings can add labelled rows.
    auto* pGrid = new QGridLayout(pGroup);

    m_pKnobReadoutEnabled = new QCheckBox(tr("Show knob adjustment readout"), pGroup);
    m_pKnobReadoutEnabled->setObjectName(QStringLiteral("knobReadoutEnabledCheckBox"));
    m_pKnobReadoutEnabled->setToolTip(
            tr("While you turn a knob, show its value in a small label that "
               "disappears about 1 s after the knob stops changing."));
    pGrid->addWidget(m_pKnobReadoutEnabled, 0, 0, 1, 2);

    m_pKnobReadoutMode = new QComboBox(pGroup);
    m_pKnobReadoutMode->setObjectName(QStringLiteral("knobReadoutModeComboBox"));
    m_pKnobReadoutMode->addItem(tr("Simple"),
            static_cast<int>(mixxx::knobreadout::Mode::Simple));
    m_pKnobReadoutMode->addItem(tr("Precise"),
            static_cast<int>(mixxx::knobreadout::Mode::Precise));
    m_pKnobReadoutMode->setToolTip(
            tr("How deck EQ knobs read. Simple: percent of the knob's travel "
               "from the centre (-100% to +100%). Precise: dB. Gains always "
               "read in dB."));
    m_pKnobReadoutModeLabel = new QLabel(tr("Mode"), pGroup);
    m_pKnobReadoutModeLabel->setBuddy(m_pKnobReadoutMode);
    const int modeRow = pGrid->rowCount();
    pGrid->addWidget(m_pKnobReadoutModeLabel, modeRow, 0);
    pGrid->addWidget(m_pKnobReadoutMode, modeRow, 1, Qt::AlignLeft);

    m_pKnobReadoutStyle = new QComboBox(pGroup);
    m_pKnobReadoutStyle->setObjectName(QStringLiteral("knobReadoutStyleComboBox"));
    m_pKnobReadoutStyle->addItem(tr("Under the knob"),
            static_cast<int>(mixxx::knobreadout::Style::UnderKnob));
    m_pKnobReadoutStyle->addItem(tr("On the knob face"),
            static_cast<int>(mixxx::knobreadout::Style::KnobFace));
    m_pKnobReadoutStyle->setToolTip(
            tr("On the knob face, the value is drawn centred on the knob without "
               "its unit, shrunk to fit. Knobs too small for it show the value "
               "under the knob."));
    m_pKnobReadoutStyleLabel = new QLabel(tr("Style"), pGroup);
    m_pKnobReadoutStyleLabel->setBuddy(m_pKnobReadoutStyle);
    const int styleRow = pGrid->rowCount();
    pGrid->addWidget(m_pKnobReadoutStyleLabel, styleRow, 0);
    pGrid->addWidget(m_pKnobReadoutStyle, styleRow, 1);
    pGrid->setColumnStretch(1, 1);

    connect(m_pKnobReadoutEnabled,
            &QCheckBox::toggled,
            this,
            &DlgPrefDJNote::setKnobReadoutDetailsEnabled);

    // Every change applies live.
    connect(m_pKnobReadoutEnabled,
            &QCheckBox::toggled,
            this,
            &DlgPrefDJNote::slotKnobReadoutControlChanged);
    for (QComboBox* pComboBox : {m_pKnobReadoutMode, m_pKnobReadoutStyle}) {
        connect(pComboBox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &DlgPrefDJNote::slotKnobReadoutControlChanged);
    }

    layout()->addWidget(pGroup);
}

void DlgPrefDJNote::setKnobReadoutDetailsEnabled(bool enabled) {
    // Mode and style.
    for (QWidget* pWidget : {static_cast<QWidget*>(m_pKnobReadoutModeLabel),
                 static_cast<QWidget*>(m_pKnobReadoutMode),
                 static_cast<QWidget*>(m_pKnobReadoutStyleLabel),
                 static_cast<QWidget*>(m_pKnobReadoutStyle)}) {
        pWidget->setEnabled(enabled);
    }
}

void DlgPrefDJNote::slotUpdate() {
    showKnobReadoutSettings(mixxx::knobreadout::loadSettings(m_pConfig));
    showKeyMatchingSettings(mixxx::keymatch::loadSettings(m_pConfig));
}

void DlgPrefDJNote::slotApply() {
    // Changes are already stored as they are made; this only makes sure.
    mixxx::knobreadout::saveSettings(m_pConfig, knobReadoutSettingsFromControls());
    mixxx::keymatch::saveSettings(m_pConfig, keyMatchingSettingsFromControls());
}

void DlgPrefDJNote::slotResetToDefaults() {
    showKnobReadoutSettings(mixxx::knobreadout::Settings());
    slotKnobReadoutControlChanged();
    showKeyMatchingSettings(mixxx::keymatch::Settings());
    slotKeyMatchingControlChanged();
}

void DlgPrefDJNote::slotKnobReadoutControlChanged() {
    if (m_showingKnobReadoutSettings) {
        return;
    }
    // The readout overlay reads the stored settings on every readout, so
    // storing them applies them immediately.
    mixxx::knobreadout::saveSettings(m_pConfig, knobReadoutSettingsFromControls());
}

void DlgPrefDJNote::showKnobReadoutSettings(const mixxx::knobreadout::Settings& settings) {
    m_showingKnobReadoutSettings = true;
    m_pKnobReadoutEnabled->setChecked(settings.enabled);
    // setChecked() doesn't emit toggled() when the box already shows the value.
    setKnobReadoutDetailsEnabled(settings.enabled);
    m_pKnobReadoutMode->setCurrentIndex(
            m_pKnobReadoutMode->findData(static_cast<int>(settings.mode)));
    m_pKnobReadoutStyle->setCurrentIndex(
            m_pKnobReadoutStyle->findData(static_cast<int>(settings.style)));
    m_showingKnobReadoutSettings = false;
}

mixxx::knobreadout::Settings DlgPrefDJNote::knobReadoutSettingsFromControls() const {
    mixxx::knobreadout::Settings settings;
    settings.enabled = m_pKnobReadoutEnabled->isChecked();
    settings.mode = static_cast<mixxx::knobreadout::Mode>(
            m_pKnobReadoutMode->currentData().toInt());
    settings.style = static_cast<mixxx::knobreadout::Style>(
            m_pKnobReadoutStyle->currentData().toInt());
    return settings;
}

void DlgPrefDJNote::createKeyMatchingGroup() {
    auto* pGroup = new QGroupBox(tr("Key matching"), this);
    pGroup->setObjectName(QStringLiteral("keyMatchingGroupBox"));
    auto* pGrid = new QGridLayout(pGroup);

    m_pKeyMatchingHighlight = new QComboBox(pGroup);
    m_pKeyMatchingHighlight->setObjectName(QStringLiteral("keyMatchingHighlightComboBox"));
    m_pKeyMatchingHighlight->addItem(tr("Off"),
            static_cast<int>(mixxx::keymatch::Highlight::Off));
    m_pKeyMatchingHighlight->addItem(tr("Strict only"),
            static_cast<int>(mixxx::keymatch::Highlight::StrictOnly));
    m_pKeyMatchingHighlight->addItem(tr("Strict and fuzzy"),
            static_cast<int>(mixxx::keymatch::Highlight::StrictAndFuzzy));
    m_pKeyMatchingHighlight->setToolTip(
            tr("Colour the cells of tracks in the track lists whose key fits "
               "the key of the deck you are mixing from. Strict matches (same key, one hour away "
               "or the relative major/minor) are green; fuzzy matches (two "
               "hours or a semitone away) are yellow."));
    auto* pHighlightLabel = new QLabel(tr("Highlight"), pGroup);
    pHighlightLabel->setBuddy(m_pKeyMatchingHighlight);
    const int row = pGrid->rowCount();
    pGrid->addWidget(pHighlightLabel, row, 0);
    pGrid->addWidget(m_pKeyMatchingHighlight, row, 1, Qt::AlignLeft);
    pGrid->setColumnStretch(1, 1);

    m_pKeyMatchingShiftedKey = new QCheckBox(
            tr("Also match the shifted key of the reference deck"), pGroup);
    m_pKeyMatchingShiftedKey->setObjectName(QStringLiteral("keyMatchingShiftedKeyCheckBox"));
    m_pKeyMatchingShiftedKey->setToolTip(
            tr("When the reference deck plays in a different key than the one "
               "stored for its track (key lock off with a tempo change, or a "
               "key shift), also colour tracks that fit that key purple."));
    pGrid->addWidget(m_pKeyMatchingShiftedKey, pGrid->rowCount(), 0, 1, 2);

    // Colour these columns: Whole row first, then the text columns.
    m_pKeyMatchingColumns = new QListWidget(pGroup);
    m_pKeyMatchingColumns->setObjectName(QStringLiteral("keyMatchingColumnsList"));
    m_pKeyMatchingColumns->setFixedHeight(140);
    m_pKeyMatchingColumns->setToolTip(
            tr("The columns that get the match colour. Whole row colours every "
               "text column; Cover, Color, Rating, Played and Preview are "
               "never coloured."));
    auto addColumnItem = [this](const QString& text, int identity) {
        auto* pItem = new QListWidgetItem(text, m_pKeyMatchingColumns);
        pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        pItem->setCheckState(Qt::Unchecked);
        pItem->setData(Qt::UserRole, identity);
    };
    addColumnItem(tr("Whole row"), kWholeRowItem);
    for (const auto& column : mixxx::keymatch::choosableColumns()) {
        addColumnItem(QCoreApplication::translate("DlgPrefDJNote", column.label),
                static_cast<int>(column.identity));
    }
    auto* pColumnsLabel = new QLabel(tr("Colour these columns"), pGroup);
    pColumnsLabel->setBuddy(m_pKeyMatchingColumns);
    pColumnsLabel->setAlignment(Qt::AlignTop);
    const int columnsRow = pGrid->rowCount();
    pGrid->addWidget(pColumnsLabel, columnsRow, 0);
    pGrid->addWidget(m_pKeyMatchingColumns, columnsRow, 1);

    m_pKeyMatchingBold = new QCheckBox(tr("Bold matched text"), pGroup);
    m_pKeyMatchingBold->setObjectName(QStringLiteral("keyMatchingBoldCheckBox"));
    m_pKeyMatchingBold->setToolTip(tr("Show the highlighted cells in bold."));
    pGrid->addWidget(m_pKeyMatchingBold, pGrid->rowCount(), 0, 1, 2);

    // Colour pickers: a swatch button per tier opens the colour dialog.
    struct ColorRow {
        mixxx::keymatch::ColorSlot slot;
        QString label;
        QString objectName;
    };
    const ColorRow colorRows[] = {
            {mixxx::keymatch::ColorSlot::Strict,
                    tr("Strict match"),
                    QStringLiteral("keyMatchingStrictColorButton")},
            {mixxx::keymatch::ColorSlot::Fuzzy,
                    tr("Fuzzy match"),
                    QStringLiteral("keyMatchingFuzzyColorButton")},
            {mixxx::keymatch::ColorSlot::ShiftedStrict,
                    tr("Shifted strict match"),
                    QStringLiteral("keyMatchingShiftedStrictColorButton")},
            {mixxx::keymatch::ColorSlot::ShiftedFuzzy,
                    tr("Shifted fuzzy match"),
                    QStringLiteral("keyMatchingShiftedFuzzyColorButton")},
    };
    for (const auto& colorRow : colorRows) {
        const int index = static_cast<int>(colorRow.slot);
        auto* pButton = new QPushButton(pGroup);
        pButton->setObjectName(colorRow.objectName);
        pButton->setFixedWidth(80);
        pButton->setToolTip(tr("Choose the colour of %1 highlights.").arg(colorRow.label.toLower()));
        auto* pLabel = new QLabel(colorRow.label, pGroup);
        pLabel->setBuddy(pButton);
        const int colorGridRow = pGrid->rowCount();
        pGrid->addWidget(pLabel, colorGridRow, 0);
        pGrid->addWidget(pButton, colorGridRow, 1, Qt::AlignLeft);
        m_keyMatchingColorButtons[index] = pButton;
        m_keyMatchingColorLabels[index] = pLabel;
        const auto slot = colorRow.slot;
        connect(pButton, &QPushButton::clicked, this, [this, slot, pButton] {
            const QColor current = pButton->property("keyMatchColor").value<QColor>();
            const QColor chosen = QColorDialog::getColor(current, this, tr("Choose colour"));
            if (chosen.isValid()) {
                pickKeyMatchingColor(slot, chosen);
            }
        });
    }
    m_pKeyMatchingResetColors = new QPushButton(tr("Reset colours"), pGroup);
    m_pKeyMatchingResetColors->setObjectName(QStringLiteral("keyMatchingResetColorsButton"));
    m_pKeyMatchingResetColors->setToolTip(tr("Restore the four default match colours."));
    pGrid->addWidget(m_pKeyMatchingResetColors, pGrid->rowCount(), 1, Qt::AlignLeft);
    connect(m_pKeyMatchingResetColors, &QPushButton::clicked, this, [this] {
        auto settings = keyMatchingSettingsFromControls();
        settings.resetColors();
        showKeyMatchingSettings(settings);
        slotKeyMatchingControlChanged();
    });

    // Every change applies live.
    connect(m_pKeyMatchingBold,
            &QCheckBox::toggled,
            this,
            &DlgPrefDJNote::slotKeyMatchingControlChanged);
    connect(m_pKeyMatchingShiftedKey,
            &QCheckBox::toggled,
            this,
            &DlgPrefDJNote::slotKeyMatchingControlChanged);
    connect(m_pKeyMatchingHighlight,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefDJNote::slotKeyMatchingControlChanged);
    connect(m_pKeyMatchingColumns,
            &QListWidget::itemChanged,
            this,
            &DlgPrefDJNote::slotKeyMatchingControlChanged);

    layout()->addWidget(pGroup);
}

void DlgPrefDJNote::slotKeyMatchingControlChanged() {
    if (m_showingKeyMatchingSettings) {
        return;
    }
    mixxx::keymatch::saveSettings(m_pConfig, keyMatchingSettingsFromControls());
    // The key matcher reloads the stored settings and repaints the track lists.
    if (auto* pMatcher = mixxx::keymatch::KeyMatcher::instance()) {
        pMatcher->reloadSettings();
    }
    updateKeyMatchingEnabled();
}

void DlgPrefDJNote::updateKeyMatchingEnabled() {
    const bool wasShowing = m_showingKeyMatchingSettings;
    m_showingKeyMatchingSettings = true; // changing flags emits itemChanged
    const auto highlight = static_cast<mixxx::keymatch::Highlight>(
            m_pKeyMatchingHighlight->currentData().toInt());
    const bool on = highlight != mixxx::keymatch::Highlight::Off;
    const bool fuzzy = highlight == mixxx::keymatch::Highlight::StrictAndFuzzy;
    const bool shifted = on && m_pKeyMatchingShiftedKey->isChecked();
    m_pKeyMatchingShiftedKey->setEnabled(on);
    m_pKeyMatchingColumns->setEnabled(on);
    m_pKeyMatchingBold->setEnabled(on);
    for (const auto slot : mixxx::keymatch::kColorSlots) {
        bool enabled = false;
        switch (slot) {
        case mixxx::keymatch::ColorSlot::Strict:
            enabled = on;
            break;
        case mixxx::keymatch::ColorSlot::Fuzzy:
            enabled = fuzzy;
            break;
        case mixxx::keymatch::ColorSlot::ShiftedStrict:
            enabled = shifted;
            break;
        case mixxx::keymatch::ColorSlot::ShiftedFuzzy:
            enabled = shifted && fuzzy;
            break;
        }
        const int index = static_cast<int>(slot);
        m_keyMatchingColorButtons[index]->setEnabled(enabled);
        m_keyMatchingColorLabels[index]->setEnabled(enabled);
    }
    m_pKeyMatchingResetColors->setEnabled(on);
    const bool wholeRow =
            m_pKeyMatchingColumns->item(0)->checkState() == Qt::Checked;
    for (int i = 1; i < m_pKeyMatchingColumns->count(); ++i) {
        auto* pItem = m_pKeyMatchingColumns->item(i);
        Qt::ItemFlags flags = pItem->flags();
        flags.setFlag(Qt::ItemIsEnabled, !wholeRow);
        pItem->setFlags(flags);
    }
    m_showingKeyMatchingSettings = wasShowing;
}

void DlgPrefDJNote::showKeyMatchingSettings(const mixxx::keymatch::Settings& settings) {
    m_showingKeyMatchingSettings = true;
    m_pKeyMatchingHighlight->setCurrentIndex(
            m_pKeyMatchingHighlight->findData(static_cast<int>(settings.highlight)));
    for (int i = 0; i < m_pKeyMatchingColumns->count(); ++i) {
        auto* pItem = m_pKeyMatchingColumns->item(i);
        const int identity = pItem->data(Qt::UserRole).toInt();
        const bool ticked = identity == kWholeRowItem
                ? settings.wholeRow
                : settings.columns.count(
                          static_cast<mixxx::keymatch::ColumnIdentity>(identity)) > 0;
        pItem->setCheckState(ticked ? Qt::Checked : Qt::Unchecked);
    }
    m_pKeyMatchingBold->setChecked(settings.boldMatchedText);
    m_pKeyMatchingShiftedKey->setChecked(settings.matchShiftedKey);
    for (const auto slot : mixxx::keymatch::kColorSlots) {
        showKeyMatchingColor(static_cast<int>(slot), settings.color(slot));
    }
    m_showingKeyMatchingSettings = false;
    updateKeyMatchingEnabled();
}

mixxx::keymatch::Settings DlgPrefDJNote::keyMatchingSettingsFromControls() const {
    mixxx::keymatch::Settings settings;
    settings.highlight = static_cast<mixxx::keymatch::Highlight>(
            m_pKeyMatchingHighlight->currentData().toInt());
    settings.columns.clear();
    for (int i = 0; i < m_pKeyMatchingColumns->count(); ++i) {
        const auto* pItem = m_pKeyMatchingColumns->item(i);
        const int identity = pItem->data(Qt::UserRole).toInt();
        const bool ticked = pItem->checkState() == Qt::Checked;
        if (identity == kWholeRowItem) {
            settings.wholeRow = ticked;
        } else if (ticked) {
            settings.columns.insert(static_cast<mixxx::keymatch::ColumnIdentity>(identity));
        }
    }
    settings.boldMatchedText = m_pKeyMatchingBold->isChecked();
    settings.matchShiftedKey = m_pKeyMatchingShiftedKey->isChecked();
    for (const auto slot : mixxx::keymatch::kColorSlots) {
        settings.color(slot) = m_keyMatchingColorButtons[static_cast<int>(slot)]
                                       ->property("keyMatchColor")
                                       .value<QColor>();
    }
    return settings;
}

void DlgPrefDJNote::showKeyMatchingColor(int index, const QColor& color) {
    QPushButton* pButton = m_keyMatchingColorButtons[index];
    pButton->setProperty("keyMatchColor", color);
    pButton->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; }")
                                   .arg(color.name(QColor::HexRgb)));
}

void DlgPrefDJNote::pickKeyMatchingColor(
        mixxx::keymatch::ColorSlot slot, const QColor& color) {
    showKeyMatchingColor(static_cast<int>(slot), color);
    slotKeyMatchingControlChanged();
}
