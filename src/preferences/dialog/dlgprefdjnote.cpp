#include "preferences/dialog/dlgprefdjnote.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "library/keymatch/keymatcher.h"

#include "moc_dlgprefdjnote.cpp"

namespace {

// The Whole row item's data, apart from every ColumnIdentity.
constexpr int kWholeRowItem = -1;

QString columnLabel(mixxx::keymatch::ColumnIdentity identity) {
    using mixxx::keymatch::ColumnIdentity;
    switch (identity) {
    case ColumnIdentity::Key:
        return DlgPrefDJNote::tr("Key");
    case ColumnIdentity::Title:
        return DlgPrefDJNote::tr("Title");
    case ColumnIdentity::Artist:
        return DlgPrefDJNote::tr("Artist");
    case ColumnIdentity::Album:
        return DlgPrefDJNote::tr("Album");
    case ColumnIdentity::AlbumArtist:
        return DlgPrefDJNote::tr("Album Artist");
    case ColumnIdentity::Year:
        return DlgPrefDJNote::tr("Year");
    case ColumnIdentity::Genre:
        return DlgPrefDJNote::tr("Genre");
    case ColumnIdentity::Composer:
        return DlgPrefDJNote::tr("Composer");
    case ColumnIdentity::Grouping:
        return DlgPrefDJNote::tr("Grouping");
    case ColumnIdentity::TrackNumber:
        return DlgPrefDJNote::tr("Track #");
    case ColumnIdentity::FileType:
        return DlgPrefDJNote::tr("Type");
    case ColumnIdentity::Comment:
        return DlgPrefDJNote::tr("Comment");
    case ColumnIdentity::Duration:
        return DlgPrefDJNote::tr("Duration");
    case ColumnIdentity::Bitrate:
        return DlgPrefDJNote::tr("Bitrate");
    case ColumnIdentity::Bpm:
        return DlgPrefDJNote::tr("BPM");
    case ColumnIdentity::ReplayGain:
        return DlgPrefDJNote::tr("ReplayGain");
    case ColumnIdentity::Location:
        return DlgPrefDJNote::tr("Location");
    case ColumnIdentity::TimesPlayed:
        return DlgPrefDJNote::tr("Played times");
    case ColumnIdentity::DateAdded:
        return DlgPrefDJNote::tr("Date Added");
    case ColumnIdentity::LastPlayed:
        return DlgPrefDJNote::tr("Last Played");
    case ColumnIdentity::NotColoured:
        break;
    }
    return QString();
}

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
          m_pKeyMatchingBold(nullptr) {
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
    // Two columns (label, control), so later settings can add labelled rows.
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
            tr("Colour the key of library tracks that fit the key of the deck "
               "you are mixing from. Strict matches (same key, one hour away "
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
    for (const auto identity : mixxx::keymatch::choosableColumns()) {
        addColumnItem(columnLabel(identity), static_cast<int>(identity));
    }
    auto* pColumnsLabel = new QLabel(tr("Colour these columns"), pGroup);
    pColumnsLabel->setBuddy(m_pKeyMatchingColumns);
    pColumnsLabel->setAlignment(Qt::AlignTop);
    const int columnsRow = pGrid->rowCount();
    pGrid->addWidget(pColumnsLabel, columnsRow, 0);
    pGrid->addWidget(m_pKeyMatchingColumns, columnsRow, 1);
    // Sits under the column checklist once that exists.
    m_pKeyMatchingBold = new QCheckBox(tr("Bold matched text"), pGroup);
    m_pKeyMatchingBold->setObjectName(QStringLiteral("keyMatchingBoldCheckBox"));
    m_pKeyMatchingBold->setToolTip(tr("Show the highlighted cells in bold."));
    pGrid->addWidget(m_pKeyMatchingBold, pGrid->rowCount(), 0, 1, 2);
    connect(m_pKeyMatchingBold,
            &QCheckBox::toggled,
            this,
            &DlgPrefDJNote::slotKeyMatchingControlChanged);

    // Every change applies live.
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
    m_pKeyMatchingShiftedKey->setEnabled(highlight != mixxx::keymatch::Highlight::Off);
    m_pKeyMatchingColumns->setEnabled(highlight != mixxx::keymatch::Highlight::Off);
    m_pKeyMatchingBold->setEnabled(highlight != mixxx::keymatch::Highlight::Off);
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
    return settings;
}
