#include "preferences/dialog/dlgprefdjnote.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "moc_dlgprefdjnote.cpp"

DlgPrefDJNote::DlgPrefDJNote(QWidget* pParent, UserSettingsPointer pConfig)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pKnobReadoutEnabled(nullptr),
          m_pKnobReadoutModeLabel(nullptr),
          m_pKnobReadoutMode(nullptr) {
    auto* pPageLayout = new QVBoxLayout(this);
    pPageLayout->setContentsMargins(0, 0, 0, 0);
    createKnobReadoutGroup();
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

    // Mode: items in Mode order, so the index is the Mode value.
    m_pKnobReadoutMode = new QComboBox(pGroup);
    m_pKnobReadoutMode->setObjectName(QStringLiteral("knobReadoutModeComboBox"));
    m_pKnobReadoutMode->addItem(tr("Simple"));
    m_pKnobReadoutMode->addItem(tr("Precise"));
    m_pKnobReadoutMode->setToolTip(
            tr("How deck EQ knobs read. Simple: percent of the knob's travel "
               "from the centre (-100% to +100%). Precise: dB. Gains always "
               "read in dB."));
    m_pKnobReadoutModeLabel = new QLabel(tr("Mode"), pGroup);
    m_pKnobReadoutModeLabel->setBuddy(m_pKnobReadoutMode);
    const int modeRow = pGrid->rowCount();
    pGrid->addWidget(m_pKnobReadoutModeLabel, modeRow, 0);
    pGrid->addWidget(m_pKnobReadoutMode, modeRow, 1, Qt::AlignLeft);
    // The mode means nothing while the readout is off.
    connect(m_pKnobReadoutEnabled,
            &QCheckBox::toggled,
            m_pKnobReadoutMode,
            &QWidget::setEnabled);
    connect(m_pKnobReadoutEnabled,
            &QCheckBox::toggled,
            m_pKnobReadoutModeLabel,
            &QWidget::setEnabled);
    pGrid->setColumnStretch(1, 1);

    layout()->addWidget(pGroup);
}

void DlgPrefDJNote::slotUpdate() {
    showKnobReadoutSettings(mixxx::knobreadout::loadSettings(m_pConfig));
}

void DlgPrefDJNote::slotApply() {
    // The readout overlay reads the stored settings on every readout, so
    // saving them applies them immediately.
    mixxx::knobreadout::saveSettings(m_pConfig, knobReadoutSettingsFromControls());
}

void DlgPrefDJNote::slotResetToDefaults() {
    showKnobReadoutSettings(mixxx::knobreadout::Settings());
}

void DlgPrefDJNote::showKnobReadoutSettings(const mixxx::knobreadout::Settings& settings) {
    m_pKnobReadoutEnabled->setChecked(settings.enabled);
    m_pKnobReadoutMode->setEnabled(settings.enabled);
    m_pKnobReadoutModeLabel->setEnabled(settings.enabled);
    m_pKnobReadoutMode->setCurrentIndex(static_cast<int>(settings.mode));
}

mixxx::knobreadout::Settings DlgPrefDJNote::knobReadoutSettingsFromControls() const {
    mixxx::knobreadout::Settings settings;
    settings.enabled = m_pKnobReadoutEnabled->isChecked();
    settings.mode = m_pKnobReadoutMode->currentIndex() ==
                    static_cast<int>(mixxx::knobreadout::Mode::Precise)
            ? mixxx::knobreadout::Mode::Precise
            : mixxx::knobreadout::Mode::Simple;
    return settings;
}
