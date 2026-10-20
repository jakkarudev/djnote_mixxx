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
          m_pKnobReadoutMode(nullptr),
          m_pKnobReadoutStyleLabel(nullptr),
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
}

void DlgPrefDJNote::slotApply() {
    // Changes are already stored as they are made; this only makes sure.
    mixxx::knobreadout::saveSettings(m_pConfig, knobReadoutSettingsFromControls());
}

void DlgPrefDJNote::slotResetToDefaults() {
    showKnobReadoutSettings(mixxx::knobreadout::Settings());
    slotKnobReadoutControlChanged();
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
