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
          m_pKnobReadoutStyle(nullptr) {
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
    auto* pStyleLabel = new QLabel(tr("Style"), pGroup);
    pStyleLabel->setBuddy(m_pKnobReadoutStyle);
    const int styleRow = pGrid->rowCount();
    pGrid->addWidget(pStyleLabel, styleRow, 0);
    pGrid->addWidget(m_pKnobReadoutStyle, styleRow, 1);
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
    m_pKnobReadoutStyle->setCurrentIndex(
            m_pKnobReadoutStyle->findData(static_cast<int>(settings.style)));
}

mixxx::knobreadout::Settings DlgPrefDJNote::knobReadoutSettingsFromControls() const {
    mixxx::knobreadout::Settings settings;
    settings.enabled = m_pKnobReadoutEnabled->isChecked();
    settings.style = static_cast<mixxx::knobreadout::Style>(
            m_pKnobReadoutStyle->currentData().toInt());
    return settings;
}
