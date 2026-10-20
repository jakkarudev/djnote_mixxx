#include "preferences/dialog/dlgprefwaveform.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaEnum>
#include <QPushButton>
#include <QSignalBlocker>

#include "control/controlpushbutton.h"
#include "library/dao/analysisdao.h"
#include "library/library.h"
#include "moc_dlgprefwaveform.cpp"
#include "preferences/waveformsettings.h"
#include "util/db/dbconnectionpooled.h"
#include "waveform/overviewlinelayout.h"
#include "waveform/phosphor/phosphorsettings.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/woverview.h"

namespace {
const ConfigKey kOverviewTypeCfgKey(QStringLiteral("[Waveform]"),
        QStringLiteral("WaveformOverviewType"));
} // namespace

DlgPrefWaveform::DlgPrefWaveform(
        QWidget* pParent,
        UserSettingsPointer pConfig,
        std::shared_ptr<Library> pLibrary)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary) {
    setupUi(this);

    // Waveform overview init
    waveformOverviewComboBox->addItem(
            tr("Filtered"), QVariant::fromValue(WOverview::Type::Filtered));
    waveformOverviewComboBox->addItem(tr("HSV"), QVariant::fromValue(WOverview::Type::HSV));
    waveformOverviewComboBox->addItem(tr("RGB"), QVariant::fromValue(WOverview::Type::RGB));
    waveformOverviewComboBox->addItem(
            tr("Phosphor"), QVariant::fromValue(WOverview::Type::Phosphor));
    m_pTypeControl = std::make_unique<ControlPushButton>(kOverviewTypeCfgKey);
    m_pTypeControl->setStates(QMetaEnum::fromType<WOverview::Type>().keyCount());
    m_pTypeControl->setReadOnly();
    // Update the control with the config value
    WOverview::Type overviewType =
            m_pConfig->getValue<WOverview::Type>(kOverviewTypeCfgKey, WOverview::Type::RGB);
    int cfgTypeIndex = waveformOverviewComboBox->findData(QVariant::fromValue(overviewType));
    if (cfgTypeIndex == -1) {
        // Invalid config value, set default type RGB and write it to config
        cfgTypeIndex = waveformOverviewComboBox->findData(
                QVariant::fromValue(WOverview::Type::RGB));
        waveformOverviewComboBox->setCurrentIndex(cfgTypeIndex);
        m_pConfig->setValue(kOverviewTypeCfgKey, cfgTypeIndex);
    } else {
        waveformOverviewComboBox->setCurrentIndex(cfgTypeIndex);
    }
    // Set the control used by WOverview
    m_pTypeControl->forceSet(cfgTypeIndex);

    // Populate waveform options.
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    // We assume that the original type list order remains constant.
    // We will use the type index later on to set waveform types and to
    // update the combobox.
    QVector<WaveformWidgetAbstractHandle> handles = factory->getAvailableTypes();
    for (int i = 0; i < handles.size(); ++i) {
        waveformTypeComboBox->addItem(handles[i].getDisplayName(), i);
    }
    // Sort the combobox items alphabetically
    waveformTypeComboBox->model()->sort(0);

    // Populate zoom options.
    for (int i = static_cast<int>(WaveformWidgetRenderer::s_waveformMinZoom);
            i <= static_cast<int>(WaveformWidgetRenderer::s_waveformMaxZoom);
            i++) {
        defaultZoomComboBox->addItem(QString::number(100 / static_cast<double>(i), 'f', 1) + " %");
    }

    // Populate untilMark options
    untilMarkAlignComboBox->addItem(tr("Top"));
    untilMarkAlignComboBox->addItem(tr("Center"));
    untilMarkAlignComboBox->addItem(tr("Bottom"));

    untilMarkTextHeightLimitComboBox->addItem(tr("1/3rd of waveform viewer"));
    untilMarkTextHeightLimitComboBox->addItem(tr("Full waveform viewer height"));

    // Populate overview lines options
    overviewLinesComboBox->addItem(tr("Bars + phrases"),
            static_cast<int>(mixxx::overviewlayout::LinesMode::BarsAndPhrases));
    overviewLinesComboBox->addItem(tr("Phrases only"),
            static_cast<int>(mixxx::overviewlayout::LinesMode::PhrasesOnly));
    overviewLinesComboBox->addItem(tr("Off"),
            static_cast<int>(mixxx::overviewlayout::LinesMode::Off));
    for (const int bars : mixxx::overviewlayout::kPhraseBarsChoices) {
        // All choices are plural; the count selects the plural form.
        overviewPhraseBarsComboBox->addItem(tr("%1 bars", "", bars).arg(bars), bars);
    }

    // Populate hover readout options
    {
        using namespace mixxx::overviewlayout;
        hoverDisplayComboBox->addItem(tr("Line + label"),
                QVariant(static_cast<int>(HoverDisplay::LineAndLabel)));
        hoverDisplayComboBox->addItem(
                tr("Line only"), QVariant(static_cast<int>(HoverDisplay::LineOnly)));
        hoverDisplayComboBox->addItem(
                tr("Label only"), QVariant(static_cast<int>(HoverDisplay::LabelOnly)));
        hoverModeComboBox->addItem(tr("Extend time ruler"),
                QVariant(static_cast<int>(HoverMode::ExtendTimeRuler)));
        hoverModeComboBox->addItem(tr("Separate hover line"),
                QVariant(static_cast<int>(HoverMode::SeparateHoverLine)));
        hoverUnitComboBox->addItem(tr("Bars"), QVariant(static_cast<int>(BeatDistanceUnit::Bars)));
        hoverUnitComboBox->addItem(tr("Beats"), QVariant(static_cast<int>(BeatDistanceUnit::Beats)));
        hoverPrecisionComboBox->addItem(tr("Whole (rounded)"),
                QVariant(static_cast<int>(BeatDistancePrecision::Whole)));
        hoverPrecisionComboBox->addItem(tr("Fractional"),
                QVariant(static_cast<int>(BeatDistancePrecision::Fractional)));
        hoverLabelPositionComboBox->addItem(
                tr("Auto"), QVariant(static_cast<int>(HoverReadoutPosition::Auto)));
        hoverLabelPositionComboBox->addItem(tr("Always right"),
                QVariant(static_cast<int>(HoverReadoutPosition::AlwaysRight)));
        hoverLabelPositionComboBox->addItem(tr("Always left"),
                QVariant(static_cast<int>(HoverReadoutPosition::AlwaysLeft)));
        hoverOverCueComboBox->addItem(
                tr("Join cue times"), QVariant(static_cast<int>(HoverOverCue::JoinCueTimes)));
        hoverOverCueComboBox->addItem(tr("Hide hover readout"),
                QVariant(static_cast<int>(HoverOverCue::HideHoverReadout)));
        hoverOverCueComboBox->addItem(
                tr("Hide cue times"), QVariant(static_cast<int>(HoverOverCue::HideCueTimes)));
        hoverOverCueComboBox->addItem(tr("Move hover readout"),
                QVariant(static_cast<int>(HoverOverCue::MoveHoverReadout)));
    }

    // slotUpdate() shows the Phosphor settings, so its controls must exist.
    phosphorDetailComboBox->addItem(
            tr("Full"), static_cast<int>(mixxx::phosphor::WaveformDetail::Full));
    phosphorDetailComboBox->addItem(
            tr("High"), static_cast<int>(mixxx::phosphor::WaveformDetail::High));
    phosphorDetailComboBox->addItem(
            tr("Medium"), static_cast<int>(mixxx::phosphor::WaveformDetail::Medium));
    phosphorDetailComboBox->addItem(
            tr("Low"), static_cast<int>(mixxx::phosphor::WaveformDetail::Low));
    buildPhosphorAdvanced();
    buildPhosphorOverviewAdvanced();

    // The GUI is not fully setup so connecting signals before calling
    // slotUpdate can generate rebootMixxxView calls.
    // TODO(XXX): Improve this awkwardness.
    slotUpdate();

    connect(frameRateSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetFrameRate);
    connect(endOfTrackWarningTimeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetWaveformEndRender);
    connect(beatGridAlphaSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetBeatGridAlpha);
    connect(frameRateSlider,
            &QSlider::valueChanged,
            frameRateSpinBox,
            &QSpinBox::setValue);
    connect(frameRateSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            frameRateSlider,
            &QSlider::setValue);
    connect(endOfTrackWarningTimeSlider,
            &QSlider::valueChanged,
            endOfTrackWarningTimeSpinBox,
            &QSpinBox::setValue);
    connect(endOfTrackWarningTimeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            endOfTrackWarningTimeSlider,
            &QSlider::setValue);
    connect(beatGridAlphaSlider,
            &QSlider::valueChanged,
            beatGridAlphaSpinBox,
            &QSpinBox::setValue);
    connect(beatGridAlphaSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            beatGridAlphaSlider,
            &QSlider::setValue);

    connect(waveformTypeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetWaveformType);
    connect(defaultZoomComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetDefaultZoom);
    connect(synchronizeZoomCheckBox,
            &QCheckBox::clicked,
            this,
            &DlgPrefWaveform::slotSetZoomSynchronization);
    connect(allVisualGain,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetVisualGainAll);
    connect(lowVisualGain,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetVisualGainLow);
    connect(midVisualGain,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetVisualGainMid);
    connect(highVisualGain,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetVisualGainHigh);
    connect(followsGainCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetFollowsGain);
    connect(followsEqCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetFollowsEq);
    // Phosphor: the detail applies from the next track load. (Its items and
    // the advanced controls are created before slotUpdate().)
    connect(phosphorDetailComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetPhosphorDetail);
    connect(normalizeOverviewCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetNormalizeOverview);
    connect(overviewLinesComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewLinesMode);
    connect(overviewPhraseBarsComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewPhraseBars);
    connect(overviewLineOpacitySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewLineOpacity);
    connect(overviewLineOpacitySlider,
            &QSlider::valueChanged,
            overviewLineOpacitySpinBox,
            &QSpinBox::setValue);
    connect(overviewLineOpacitySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            overviewLineOpacitySlider,
            &QSlider::setValue);
    connect(factory,
            &WaveformWidgetFactory::waveformMeasured,
            this,
            &DlgPrefWaveform::slotWaveformMeasured);
    connect(waveformOverviewComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetWaveformOverviewType);
    connect(clearCachedWaveforms,
            &QAbstractButton::clicked,
            this,
            &DlgPrefWaveform::slotClearCachedWaveforms);
    connect(playMarkerPositionSlider,
            &QSlider::valueChanged,
            this,
            &DlgPrefWaveform::slotSetPlayMarkerPosition);
    connect(untilMarkShowBeatsCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetUntilMarkShowBeats);
    connect(untilMarkShowTimeCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetUntilMarkShowTime);
    connect(untilMarkAlignComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetUntilMarkAlign);
    connect(untilMarkTextPointSizeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefWaveform::slotSetUntilMarkTextPointSize);
    connect(untilMarkTextHeightLimitComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetUntilMarkTextHeightLimit);

    connect(showHoverReadoutCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverDisplayComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverModeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverUnitComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverPrecisionComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverLabelPositionComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(hoverOverCueComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);
    connect(snapSeeksCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefWaveform::slotSetOverviewHoverReadout);

    setScrollSafeGuardForAllInputWidgets(this);
}

DlgPrefWaveform::~DlgPrefWaveform() {
}

void DlgPrefWaveform::slotUpdate() {
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();

    if (factory->isOpenGlAvailable() || factory->isOpenGlesAvailable()) {
        openGlStatusData->setText(factory->getOpenGLVersion());
    } else {
        openGlStatusData->setText(tr("OpenGL not available") + ": " + factory->getOpenGLVersion());
    }

    // The combobox holds a list of [handle name, handle index]
    int currentIndex = waveformTypeComboBox->findData(factory->getHandleIndex());
    if (currentIndex != -1 && waveformTypeComboBox->currentIndex() != currentIndex) {
        waveformTypeComboBox->setCurrentIndex(currentIndex);
    }

    updateEnableUntilMark();

    frameRateSpinBox->setValue(factory->getFrameRate());
    frameRateSlider->setValue(factory->getFrameRate());
    endOfTrackWarningTimeSpinBox->setValue(factory->getEndOfTrackWarningTime());
    endOfTrackWarningTimeSlider->setValue(factory->getEndOfTrackWarningTime());
    synchronizeZoomCheckBox->setChecked(factory->isZoomSync());
    allVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::All));
    lowVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::Low));
    midVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::Mid));
    highVisualGain->setValue(factory->getVisualGain(WaveformWidgetFactory::High));
    followsGainCheckBox->setChecked(factory->followsGain());
    followsEqCheckBox->setChecked(factory->followsEq());
    showPhosphorSettings(factory->phosphorSettings());
    showPhosphorOverviewSettings(factory->phosphorOverviewSettings());
    normalizeOverviewCheckBox->setChecked(factory->isOverviewNormalized());
    overviewLinesComboBox->setCurrentIndex(overviewLinesComboBox->findData(
            static_cast<int>(factory->getOverviewLineSettings().mode)));
    overviewPhraseBarsComboBox->setCurrentIndex(overviewPhraseBarsComboBox->findData(
            factory->getOverviewLineSettings().phraseBars));
    overviewLineOpacitySpinBox->setValue(factory->getOverviewLineSettings().opacity.percent());
    overviewLineOpacitySlider->setValue(factory->getOverviewLineSettings().opacity.percent());
    // Round zoom to int to get a default zoom index.
    defaultZoomComboBox->setCurrentIndex(static_cast<int>(factory->getDefaultZoom()) - 1);
    playMarkerPositionSlider->setValue(static_cast<int>(factory->getPlayMarkerPosition() * 100));
    beatGridAlphaSpinBox->setValue(factory->getBeatGridAlpha());
    beatGridAlphaSlider->setValue(factory->getBeatGridAlpha());

    untilMarkShowBeatsCheckBox->setChecked(factory->getUntilMarkShowBeats());
    untilMarkShowTimeCheckBox->setChecked(factory->getUntilMarkShowTime());
    untilMarkAlignComboBox->setCurrentIndex(
            WaveformWidgetFactory::toUntilMarkAlignIndex(
                    factory->getUntilMarkAlign()));
    untilMarkTextPointSizeSpinBox->setValue(factory->getUntilMarkTextPointSize());
    untilMarkTextHeightLimitComboBox->setCurrentIndex(
            WaveformWidgetFactory::toUntilMarkTextHeightLimitIndex(
                    factory->getUntilMarkTextHeightLimit()));

    showHoverReadoutControls(factory->getOverviewHoverReadoutSettings());

    WOverview::Type cfgOverviewType =
            m_pConfig->getValue<WOverview::Type>(kOverviewTypeCfgKey, WOverview::Type::RGB);
    // Assumes the combobox index is in sync with the ControlPushButton
    if (cfgOverviewType != waveformOverviewComboBox->currentData().value<WOverview::Type>()) {
        int cfgOverviewTypeIndex =
                waveformOverviewComboBox->findData(QVariant::fromValue(cfgOverviewType));
        waveformOverviewComboBox->setCurrentIndex(cfgOverviewTypeIndex);
    }

    WaveformSettings waveformSettings(m_pConfig);
    enableWaveformCaching->setChecked(waveformSettings.waveformCachingEnabled());
    enableWaveformGenerationWithAnalysis->setChecked(
        waveformSettings.waveformGenerationWithAnalysisEnabled());
    calculateCachedWaveformDiskUsage();
}

void DlgPrefWaveform::slotApply() {
    WaveformSettings waveformSettings(m_pConfig);
    waveformSettings.setWaveformCachingEnabled(enableWaveformCaching->isChecked());
    waveformSettings.setWaveformGenerationWithAnalysisEnabled(
        enableWaveformGenerationWithAnalysis->isChecked());
}

void DlgPrefWaveform::slotResetToDefaults() {
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();

    // Get the default we ought to use based on whether the user has OpenGL or not.
    // Select the combobox index that holds the default handle's index in data column.
    int defaultIndex = waveformTypeComboBox->findData(
            factory->findHandleIndexFromType(factory->autoChooseWidgetType()));
    if (defaultIndex != -1 && waveformTypeComboBox->currentIndex() != defaultIndex) {
        waveformTypeComboBox->setCurrentIndex(defaultIndex);
    }

    allVisualGain->setValue(1.0);
    lowVisualGain->setValue(1.0);
    midVisualGain->setValue(1.0);
    highVisualGain->setValue(1.0);

    // Default zoom level is 3 in WaveformWidgetFactory.
    defaultZoomComboBox->setCurrentIndex(3 + 1);

    synchronizeZoomCheckBox->setChecked(true);

    // RGB overview.
    waveformOverviewComboBox->setCurrentIndex(
            waveformOverviewComboBox->findData(QVariant::fromValue(WOverview::Type::RGB)));

    // Waveforms follow the deck's gain and EQ.
    followsGainCheckBox->setChecked(true);
    followsEqCheckBox->setChecked(true);

    // Phosphor: the defaults of PhosphorSettings.
    WaveformWidgetFactory::instance()->setPhosphorSettings(mixxx::phosphor::PhosphorSettings());
    showPhosphorSettings(mixxx::phosphor::PhosphorSettings());
    // The Phosphor overview: the defaults of PhosphorOverviewSettings.
    WaveformWidgetFactory::instance()->setPhosphorOverviewSettings(
            mixxx::phosphor::PhosphorOverviewSettings());
    showPhosphorOverviewSettings(mixxx::phosphor::PhosphorOverviewSettings());

    // Don't normalize overview.
    normalizeOverviewCheckBox->setChecked(false);

    // Overview lines: the defaults of LineSettings.
    const mixxx::overviewlayout::LineSettings defaultLineSettings;
    overviewLinesComboBox->setCurrentIndex(
            overviewLinesComboBox->findData(static_cast<int>(defaultLineSettings.mode)));
    overviewPhraseBarsComboBox->setCurrentIndex(
            overviewPhraseBarsComboBox->findData(defaultLineSettings.phraseBars));
    overviewLineOpacitySlider->setValue(defaultLineSettings.opacity.percent());
    overviewLineOpacitySpinBox->setValue(defaultLineSettings.opacity.percent());

    // 60FPS is the default
    frameRateSlider->setValue(60);
    endOfTrackWarningTimeSlider->setValue(30);

    // Waveform caching enabled.
    enableWaveformCaching->setChecked(true);
    enableWaveformGenerationWithAnalysis->setChecked(false);

    // Beat grid alpha default is 90
    beatGridAlphaSlider->setValue(90);
    beatGridAlphaSpinBox->setValue(90);

    // 50 (center) is default
    playMarkerPositionSlider->setValue(50);

    // Hover readout: the defaults of HoverReadoutSettings.
    showHoverReadoutControls(mixxx::overviewlayout::HoverReadoutSettings());
}

void DlgPrefWaveform::showHoverReadoutControls(
        const mixxx::overviewlayout::HoverReadoutSettings& settings) {
    showHoverReadoutCheckBox->setChecked(settings.showHoverReadout);
    hoverDisplayComboBox->setCurrentIndex(
            hoverDisplayComboBox->findData(static_cast<int>(settings.display)));
    hoverModeComboBox->setCurrentIndex(
            hoverModeComboBox->findData(static_cast<int>(settings.hoverMode)));
    hoverUnitComboBox->setCurrentIndex(
            hoverUnitComboBox->findData(static_cast<int>(settings.unit)));
    hoverPrecisionComboBox->setCurrentIndex(
            hoverPrecisionComboBox->findData(static_cast<int>(settings.precision)));
    hoverLabelPositionComboBox->setCurrentIndex(
            hoverLabelPositionComboBox->findData(static_cast<int>(settings.position)));
    hoverOverCueComboBox->setCurrentIndex(
            hoverOverCueComboBox->findData(static_cast<int>(settings.overCue)));
    snapSeeksCheckBox->setChecked(settings.snapSeeks);
}

void DlgPrefWaveform::slotSetOverviewHoverReadout() {
    using namespace mixxx::overviewlayout;
    HoverReadoutSettings settings;
    settings.showHoverReadout = showHoverReadoutCheckBox->isChecked();
    settings.display = static_cast<HoverDisplay>(hoverDisplayComboBox->currentData().toInt());
    settings.hoverMode = static_cast<HoverMode>(hoverModeComboBox->currentData().toInt());
    settings.unit = static_cast<BeatDistanceUnit>(hoverUnitComboBox->currentData().toInt());
    settings.precision =
            static_cast<BeatDistancePrecision>(hoverPrecisionComboBox->currentData().toInt());
    settings.position =
            static_cast<HoverReadoutPosition>(hoverLabelPositionComboBox->currentData().toInt());
    settings.overCue = static_cast<HoverOverCue>(hoverOverCueComboBox->currentData().toInt());
    settings.snapSeeks = snapSeeksCheckBox->isChecked();
    WaveformWidgetFactory::instance()->setOverviewHoverReadoutSettings(settings);
}

void DlgPrefWaveform::slotSetFrameRate(int frameRate) {
    WaveformWidgetFactory::instance()->setFrameRate(frameRate);
}

void DlgPrefWaveform::slotSetWaveformEndRender(int endTime) {
    WaveformWidgetFactory::instance()->setEndOfTrackWarningTime(endTime);
}

void DlgPrefWaveform::slotSetWaveformType(int index) {
    // Ignore sets for -1 since this happens when we clear the combobox.
    if (index < 0) {
        return;
    }
    int handleIndex = waveformTypeComboBox->itemData(index).toInt();
    WaveformWidgetFactory::instance()->setWidgetTypeFromHandle(handleIndex);

    updateEnableUntilMark();
}

void DlgPrefWaveform::updateEnableUntilMark() {
    const bool enabled = WaveformWidgetFactory::instance()->widgetTypeSupportsUntilMark();
    untilMarkShowBeatsCheckBox->setEnabled(enabled);
    untilMarkShowTimeCheckBox->setEnabled(enabled);
    untilMarkAlignLabel->setEnabled(enabled);
    untilMarkAlignComboBox->setEnabled(enabled);
    untilMarkTextPointSizeLabel->setEnabled(enabled);
    untilMarkTextPointSizeSpinBox->setEnabled(enabled);
    untilMarkTextHeightLimitComboBox->setEnabled(enabled);
    requiresGLSLLabel->setVisible(!enabled);
}

void DlgPrefWaveform::slotSetWaveformOverviewType() {
    // Apply immediately
    QVariant comboboxData = waveformOverviewComboBox->currentData();
    DEBUG_ASSERT(comboboxData.canConvert<WOverview::Type>());
    auto type = comboboxData.value<WOverview::Type>();
    m_pConfig->setValue(kOverviewTypeCfgKey, type);
    m_pTypeControl->forceSet(static_cast<double>(type));
    // Config is written first: the analyzers read it on their own thread.
    emit WaveformWidgetFactory::instance()->overviewTypeChanged();
}

void DlgPrefWaveform::slotSetDefaultZoom(int index) {
    WaveformWidgetFactory::instance()->setDefaultZoom(index + 1);
}

void DlgPrefWaveform::slotSetZoomSynchronization(bool checked) {
    WaveformWidgetFactory::instance()->setZoomSync(checked);
}

void DlgPrefWaveform::slotSetVisualGainAll(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::All,gain);
}

void DlgPrefWaveform::slotSetVisualGainLow(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::Low,gain);
}

void DlgPrefWaveform::slotSetVisualGainMid(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::Mid,gain);
}

void DlgPrefWaveform::slotSetVisualGainHigh(double gain) {
    WaveformWidgetFactory::instance()->setVisualGain(WaveformWidgetFactory::High,gain);
}

void DlgPrefWaveform::slotSetFollowsGain(bool follows) {
    WaveformWidgetFactory::instance()->setFollowsGain(follows);
}

void DlgPrefWaveform::slotSetFollowsEq(bool follows) {
    WaveformWidgetFactory::instance()->setFollowsEq(follows);
}

namespace {

QDoubleSpinBox* makeSpinBox(double min, double max, double step, int decimals, const QString& suffix) {
    auto* pBox = new QDoubleSpinBox();
    pBox->setRange(min, max);
    pBox->setSingleStep(step);
    pBox->setDecimals(decimals);
    pBox->setSuffix(suffix);
    return pBox;
}

} // namespace

void DlgPrefWaveform::buildPhosphorAdvanced() {
    using namespace mixxx::phosphor;
    const QString kNextLoad = tr("Applies from the next track you load.");
    const QString kAfterRedraw = tr(
            "Applies after the waveform has been recalculated, a moment after you change it.");

    m_pPhosphorAdvancedCheckBox = new QCheckBox(tr("Show advanced settings"));
    m_pPhosphorAdvancedCheckBox->setToolTip(
            tr("Show every calibrated value of the Phosphor waveform. The defaults are "
               "tuned to look like the reference waveform, so you rarely need to change them."));
    phosphorGridLayout->addWidget(m_pPhosphorAdvancedCheckBox, 1, 0, 1, 2);

    m_pPhosphorAdvancedWidget = new QWidget();
    auto* pGrid = new QGridLayout(m_pPhosphorAdvancedWidget);
    pGrid->setContentsMargins(0, 0, 0, 0);
    pGrid->setColumnStretch(1, 1);
    phosphorGridLayout->addWidget(m_pPhosphorAdvancedWidget, 2, 0, 1, 2);
    m_pPhosphorAdvancedWidget->setVisible(false);
    connect(m_pPhosphorAdvancedCheckBox,
            &QCheckBox::toggled,
            m_pPhosphorAdvancedWidget,
            &QWidget::setVisible);

    int row = 0;
    // One setting: a name with its control(s), and the explanation below,
    // which is also the tooltip of everything on the row.
    const auto addSetting = [&](const QString& name,
                                    const QList<QWidget*>& controls,
                                    const QString& explanation) {
        auto* pLabel = new QLabel(name);
        auto* pControls = new QHBoxLayout();
        pControls->setContentsMargins(0, 0, 0, 0);
        for (QWidget* pControl : controls) {
            pControl->setToolTip(explanation);
            pControls->addWidget(pControl);
        }
        pControls->addStretch(1);
        pLabel->setToolTip(explanation);
        pGrid->addWidget(pLabel, row, 0);
        pGrid->addLayout(pControls, row, 1);
        auto* pExplanation = new QLabel(explanation);
        pExplanation->setWordWrap(true);
        pExplanation->setToolTip(explanation);
        QFont font = pExplanation->font();
        font.setItalic(true);
        pExplanation->setFont(font);
        pGrid->addWidget(pExplanation, row + 1, 0, 1, 2);
        row += 2;
    };
    const auto changed = [this](QDoubleSpinBox* pBox) {
        connect(pBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &DlgPrefWaveform::slotPhosphorAdvancedChanged);
        return pBox;
    };
    // A small name in front of a box, for rows with three boxes.
    const auto labelled = [](const QString& text, QDoubleSpinBox* pBox) {
        auto* pWidget = new QWidget();
        auto* pLayout = new QHBoxLayout(pWidget);
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->addWidget(new QLabel(text));
        pLayout->addWidget(pBox);
        return pWidget;
    };

    m_pPhosphorSaturation = changed(makeSpinBox(kMinSaturation, kMaxSaturation, 0.1, 1, QString()));
    addSetting(tr("Saturation"),
            {m_pPhosphorSaturation},
            tr("How vivid the colors are. Higher values make mixed sounds look more like "
               "their dominant band."));

    const QString gainText = tr(
            "How strong each color is: red is the bass, green the mids, blue the highs. "
            "Turn one up to make that part of the music stand out, or down to calm it.");
    const QString gainNames[3] = {tr("Red"), tr("Green"), tr("Blue")};
    QList<QWidget*> gainWidgets;
    for (int c = 0; c < 3; ++c) {
        m_pPhosphorGain[c] = changed(makeSpinBox(0.0, kMaxChannelGain, 0.05, 2, QString()));
        m_pPhosphorGain[c]->setToolTip(gainText);
        gainWidgets.append(labelled(gainNames[c], m_pPhosphorGain[c]));
    }
    addSetting(tr("Color strength"), gainWidgets, gainText);

    m_pPhosphorBrightnessExponent = changed(makeSpinBox(
            kMinBrightnessExponent, kMaxBrightnessExponent, 0.01, 2, QString()));
    addSetting(tr("Brightness curve"),
            {m_pPhosphorBrightnessExponent},
            tr("How bright quiet sounds look next to loud ones. Lower values make quiet "
               "sounds look brighter; higher values keep only the loudest parts bright."));

    m_pPhosphorDensityBrightness = changed(makeSpinBox(
            kMinDensityBrightness, kMaxDensityBrightness, 0.05, 2, QString()));
    addSetting(tr("Overall brightness"),
            {m_pPhosphorDensityBrightness},
            tr("How bright the whole waveform is. Raise it for a bolder, brighter waveform; "
               "lower it for a subtler one."));

    m_pPhosphorBlend = new QComboBox();
    m_pPhosphorBlend->addItem(tr("Add"), static_cast<int>(LightBlend::Add));
    m_pPhosphorBlend->addItem(tr("Opaque"), static_cast<int>(LightBlend::Opaque));
    connect(m_pPhosphorBlend,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefWaveform::slotPhosphorAdvancedChanged);
    addSetting(tr("Light blend"),
            {m_pPhosphorBlend},
            tr("Add puts the waveform on top of what is behind it as light (beat grid, "
               "background). Opaque paints the waveform over the background instead."));

    m_pPhosphorShapeWindow = changed(makeSpinBox(
            kMinShapeWindow, kMaxShapeWindow, 0.5, 1, tr(" columns")));
    addSetting(tr("Outline smoothing"),
            {m_pPhosphorShapeWindow},
            tr("How many screen columns the outline of the waveform looks across. Higher values "
               "smooth out small spikes but blur fine detail. ") +
                    kAfterRedraw);

    m_pPhosphorMixExponent = changed(makeSpinBox(
            kMinMixExponent, kMaxMixExponent, 0.01, 2, QString()));
    addSetting(tr("Color mix"),
            {m_pPhosphorMixExponent},
            tr("How much quiet sounds color the waveform compared to loud ones. Lower values let "
               "quiet sounds tint the color as much as loud ones; higher values let the loudest "
               "band take over. ") +
                    kNextLoad);

    m_pPhosphorAttack = changed(makeSpinBox(0.0, kMaxAttackMs, 1.0, 0, tr(" ms")));
    addSetting(tr("Color attack"),
            {m_pPhosphorAttack},
            tr("How quickly a color appears when a sound starts. 0 shows it instantly; higher "
               "values fade it in. ") +
                    kNextLoad);

    const QString releaseText = tr(
            "How long each color lingers after its sound has ended. Longer values give "
            "smoother colors, shorter values give sharper changes. ") +
            kNextLoad;
    const QString releaseNames[3] = {tr("Low"), tr("Mid"), tr("High")};
    QList<QWidget*> releaseWidgets;
    for (int c = 0; c < 3; ++c) {
        m_pPhosphorRelease[c] = changed(makeSpinBox(0.0, kMaxReleaseMs, 10.0, 0, tr(" ms")));
        m_pPhosphorRelease[c]->setToolTip(releaseText);
        releaseWidgets.append(labelled(releaseNames[c], m_pPhosphorRelease[c]));
    }
    addSetting(tr("Color release"), releaseWidgets, releaseText);

    m_pPhosphorLookAhead = changed(makeSpinBox(0.0, kMaxLookAheadMs, 10.0, 0, tr(" ms")));
    addSetting(tr("Red look-ahead"),
            {m_pPhosphorLookAhead},
            tr("How early the red (bass) color starts to show before a kick or bass note, so "
               "you can see it coming. 0 turns it off. ") +
                    kNextLoad);

    m_pPhosphorResetButton = new QPushButton(tr("Reset to calibrated defaults"));
    m_pPhosphorResetButton->setToolTip(
            tr("Put every advanced Phosphor setting back to its calibrated value."));
    connect(m_pPhosphorResetButton,
            &QPushButton::clicked,
            this,
            &DlgPrefWaveform::slotResetPhosphorAdvanced);
    pGrid->addWidget(m_pPhosphorResetButton, row, 0, 1, 2, Qt::AlignLeft);
}

void DlgPrefWaveform::showPhosphorSettings(const mixxx::phosphor::PhosphorSettings& settings) {
    phosphorDetailComboBox->setCurrentIndex(
            phosphorDetailComboBox->findData(static_cast<int>(settings.detail)));
    // Showing a value must not save it (or apply it) again.
    const auto set = [](QDoubleSpinBox* pBox, double value) {
        const QSignalBlocker blocker(pBox);
        pBox->setValue(value);
    };
    set(m_pPhosphorSaturation, settings.saturation);
    for (int c = 0; c < 3; ++c) {
        set(m_pPhosphorGain[c], settings.channelGains[c]);
        set(m_pPhosphorRelease[c], settings.color.releaseMs[c]);
    }
    set(m_pPhosphorBrightnessExponent, settings.brightnessExponent);
    set(m_pPhosphorDensityBrightness, settings.densityBrightness);
    {
        const QSignalBlocker blocker(m_pPhosphorBlend);
        m_pPhosphorBlend->setCurrentIndex(
                m_pPhosphorBlend->findData(static_cast<int>(settings.blend)));
    }
    set(m_pPhosphorShapeWindow, settings.shapeWindow);
    set(m_pPhosphorMixExponent, settings.color.mixExponent);
    set(m_pPhosphorAttack, settings.color.attackMs);
    set(m_pPhosphorLookAhead, settings.color.lookAheadMs);
}

void DlgPrefWaveform::slotSetPhosphorDetail(int index) {
    auto* pFactory = WaveformWidgetFactory::instance();
    mixxx::phosphor::PhosphorSettings settings = pFactory->phosphorSettings();
    settings.detail = mixxx::phosphor::detailFromInt(phosphorDetailComboBox->itemData(index).toInt());
    pFactory->setPhosphorSettings(settings);
}

void DlgPrefWaveform::slotPhosphorAdvancedChanged() {
    auto* pFactory = WaveformWidgetFactory::instance();
    mixxx::phosphor::PhosphorSettings settings = pFactory->phosphorSettings();
    settings.saturation = m_pPhosphorSaturation->value();
    for (int c = 0; c < 3; ++c) {
        settings.channelGains[c] = m_pPhosphorGain[c]->value();
        settings.color.releaseMs[c] = m_pPhosphorRelease[c]->value();
    }
    settings.brightnessExponent = m_pPhosphorBrightnessExponent->value();
    settings.densityBrightness = m_pPhosphorDensityBrightness->value();
    settings.blend = mixxx::phosphor::blendFromInt(m_pPhosphorBlend->currentData().toInt());
    settings.shapeWindow = m_pPhosphorShapeWindow->value();
    settings.color.mixExponent = m_pPhosphorMixExponent->value();
    settings.color.attackMs = m_pPhosphorAttack->value();
    settings.color.lookAheadMs = m_pPhosphorLookAhead->value();
    pFactory->setPhosphorSettings(settings);
}

void DlgPrefWaveform::slotResetPhosphorAdvanced() {
    auto* pFactory = WaveformWidgetFactory::instance();
    // The waveform detail is not an advanced setting: it stays.
    mixxx::phosphor::PhosphorSettings settings;
    settings.detail = pFactory->phosphorSettings().detail;
    pFactory->setPhosphorSettings(settings);
    showPhosphorSettings(settings);
}

void DlgPrefWaveform::buildPhosphorOverviewAdvanced() {
    using namespace mixxx::phosphor;
    // Behind the same "Show advanced settings" toggle as the scrolling group,
    // under its own heading.
    m_pPhosphorOverviewWidget = new QWidget();
    auto* pGrid = new QGridLayout(m_pPhosphorOverviewWidget);
    pGrid->setContentsMargins(0, 0, 0, 0);
    pGrid->setColumnStretch(1, 1);
    phosphorGridLayout->addWidget(m_pPhosphorOverviewWidget, 3, 0, 1, 2);
    m_pPhosphorOverviewWidget->setVisible(false);
    connect(m_pPhosphorAdvancedCheckBox,
            &QCheckBox::toggled,
            m_pPhosphorOverviewWidget,
            &QWidget::setVisible);

    int row = 0;
    auto* pHeading = new QLabel(tr("Phosphor overview"));
    QFont headingFont = pHeading->font();
    headingFont.setBold(true);
    pHeading->setFont(headingFont);
    pHeading->setToolTip(
            tr("Settings of the Phosphor overview. They apply at once and never need "
               "a track to be analyzed again."));
    pGrid->addWidget(pHeading, row++, 0, 1, 2);

    const auto addSetting = [&](const QString& name,
                                    const QList<QWidget*>& controls,
                                    const QString& explanation) {
        auto* pLabel = new QLabel(name);
        auto* pControls = new QHBoxLayout();
        pControls->setContentsMargins(0, 0, 0, 0);
        for (QWidget* pControl : controls) {
            pControl->setToolTip(explanation);
            pControls->addWidget(pControl);
        }
        pControls->addStretch(1);
        pLabel->setToolTip(explanation);
        pGrid->addWidget(pLabel, row, 0);
        pGrid->addLayout(pControls, row, 1);
        auto* pExplanation = new QLabel(explanation);
        pExplanation->setWordWrap(true);
        pExplanation->setToolTip(explanation);
        QFont font = pExplanation->font();
        font.setItalic(true);
        pExplanation->setFont(font);
        pGrid->addWidget(pExplanation, row + 1, 0, 1, 2);
        row += 2;
    };
    const auto make = [this](const QString& objectName, double min, double max, double step) {
        auto* pBox = makeSpinBox(min, max, step, 2, QString());
        pBox->setObjectName(objectName);
        connect(pBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &DlgPrefWaveform::slotPhosphorOverviewChanged);
        return pBox;
    };

    m_pOverviewBrightness = make(QStringLiteral("phosphorOverviewBrightness"),
            kMinOverviewBrightness,
            kMaxOverviewBrightness,
            0.05);
    addSetting(tr("Brightness"),
            {m_pOverviewBrightness},
            tr("How bright the whole overview is. Raise it for a bolder overview; lower it "
               "for a subtler one."));

    m_pOverviewLevelExponent = make(QStringLiteral("phosphorOverviewLevelExponent"),
            kMinOverviewLevelExponent,
            kMaxOverviewLevelExponent,
            0.01);
    addSetting(tr("Level exponent"),
            {m_pOverviewLevelExponent},
            tr("How much louder passages brighten. Lower values make quiet passages look "
               "brighter; higher values keep only the loudest passages bright."));

    m_pOverviewDensityExponent = make(QStringLiteral("phosphorOverviewDensityExponent"),
            kMinOverviewDensityExponent,
            kMaxOverviewDensityExponent,
            0.01);
    addSetting(tr("Density exponent"),
            {m_pOverviewDensityExponent},
            tr("How strongly the density inside a column shows. Lower values look smoother; "
               "higher values look grainier, with a brighter center."));

    const QString gainText = tr(
            "How strong each color is: red is the bass, green the mids, blue the highs. "
            "Turn one up to make that part of the music stand out, or down to calm it.");
    const QString gainNames[3] = {tr("Low"), tr("Mid"), tr("High")};
    const char* gainObjects[3] = {"phosphorOverviewGainLow",
            "phosphorOverviewGainMid",
            "phosphorOverviewGainHigh"};
    QList<QWidget*> gainWidgets;
    for (int c = 0; c < 3; ++c) {
        m_pOverviewGain[c] = make(QString::fromLatin1(gainObjects[c]), 0.0, kMaxOverviewGain, 0.05);
        m_pOverviewGain[c]->setToolTip(gainText);
        auto* pWidget = new QWidget();
        auto* pLayout = new QHBoxLayout(pWidget);
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->addWidget(new QLabel(gainNames[c]));
        pLayout->addWidget(m_pOverviewGain[c]);
        gainWidgets.append(pWidget);
    }
    addSetting(tr("Color strength"), gainWidgets, gainText);

    m_pOverviewResetButton = new QPushButton(tr("Reset to calibrated defaults"));
    m_pOverviewResetButton->setObjectName(QStringLiteral("phosphorOverviewResetButton"));
    m_pOverviewResetButton->setToolTip(
            tr("Put every Phosphor overview setting back to its calibrated value."));
    connect(m_pOverviewResetButton,
            &QPushButton::clicked,
            this,
            &DlgPrefWaveform::slotResetPhosphorOverview);
    pGrid->addWidget(m_pOverviewResetButton, row, 0, 1, 2, Qt::AlignLeft);
}

void DlgPrefWaveform::showPhosphorOverviewSettings(
        const mixxx::phosphor::PhosphorOverviewSettings& settings) {
    // Showing a value must not save it (or apply it) again.
    const auto set = [](QDoubleSpinBox* pBox, double value) {
        const QSignalBlocker blocker(pBox);
        pBox->setValue(value);
    };
    set(m_pOverviewBrightness, settings.brightness);
    set(m_pOverviewLevelExponent, settings.levelExponent);
    set(m_pOverviewDensityExponent, settings.densityExponent);
    set(m_pOverviewGain[0], settings.gainLow);
    set(m_pOverviewGain[1], settings.gainMid);
    set(m_pOverviewGain[2], settings.gainHigh);
}

void DlgPrefWaveform::slotPhosphorOverviewChanged() {
    mixxx::phosphor::PhosphorOverviewSettings settings;
    settings.brightness = m_pOverviewBrightness->value();
    settings.levelExponent = m_pOverviewLevelExponent->value();
    settings.densityExponent = m_pOverviewDensityExponent->value();
    settings.gainLow = m_pOverviewGain[0]->value();
    settings.gainMid = m_pOverviewGain[1]->value();
    settings.gainHigh = m_pOverviewGain[2]->value();
    WaveformWidgetFactory::instance()->setPhosphorOverviewSettings(settings);
}

void DlgPrefWaveform::slotResetPhosphorOverview() {
    const mixxx::phosphor::PhosphorOverviewSettings settings;
    WaveformWidgetFactory::instance()->setPhosphorOverviewSettings(settings);
    showPhosphorOverviewSettings(settings);
}

void DlgPrefWaveform::slotSetNormalizeOverview(bool normalize) {
    WaveformWidgetFactory::instance()->setOverviewNormalized(normalize);
}

void DlgPrefWaveform::slotSetOverviewLinesMode(int index) {
    WaveformWidgetFactory::instance()->setOverviewLinesMode(
            static_cast<mixxx::overviewlayout::LinesMode>(
                    overviewLinesComboBox->itemData(index).toInt()));
}

void DlgPrefWaveform::slotSetOverviewPhraseBars(int index) {
    WaveformWidgetFactory::instance()->setOverviewPhraseBars(
            overviewPhraseBarsComboBox->itemData(index).toInt());
}

void DlgPrefWaveform::slotSetOverviewLineOpacity(int percent) {
    WaveformWidgetFactory::instance()->setOverviewLineOpacity(percent);
}

void DlgPrefWaveform::slotWaveformMeasured(float frameRate, int droppedFrames) {
    frameRateAverage->setText(
            QString::number((double)frameRate, 'f', 2) + " : " +
            tr("dropped frames") + " " + QString::number(droppedFrames));
}

void DlgPrefWaveform::slotClearCachedWaveforms() {
    AnalysisDao analysisDao(m_pConfig);
    QSqlDatabase dbConnection = mixxx::DbConnectionPooled(m_pLibrary->dbConnectionPool());
    analysisDao.deleteAnalysesByType(dbConnection, AnalysisDao::TYPE_WAVEFORM);
    analysisDao.deleteAnalysesByType(dbConnection, AnalysisDao::TYPE_WAVESUMMARY);
    calculateCachedWaveformDiskUsage();
}

void DlgPrefWaveform::slotSetBeatGridAlpha(int alpha) {
    // TODO(xxx) For consistency set this in WaveformWidgetFactory like
    // the other waveform controls.
    m_pConfig->setValue(ConfigKey("[Waveform]", "beatGridAlpha"), alpha);
    WaveformWidgetFactory::instance()->setDisplayBeatGridAlpha(alpha);
}

void DlgPrefWaveform::slotSetPlayMarkerPosition(int position) {
    // QSlider works with integer values, so divide the percentage given by the
    // slider value by 100 to get a fraction of the waveform width.
    WaveformWidgetFactory::instance()->setPlayMarkerPosition(position / 100.0);
}

void DlgPrefWaveform::slotSetUntilMarkShowBeats(bool checked) {
    WaveformWidgetFactory::instance()->setUntilMarkShowBeats(checked);
}

void DlgPrefWaveform::slotSetUntilMarkShowTime(bool checked) {
    WaveformWidgetFactory::instance()->setUntilMarkShowTime(checked);
}

void DlgPrefWaveform::slotSetUntilMarkAlign(int index) {
    WaveformWidgetFactory::instance()->setUntilMarkAlign(
            WaveformWidgetFactory::toUntilMarkAlign(index));
}

void DlgPrefWaveform::slotSetUntilMarkTextPointSize(int value) {
    WaveformWidgetFactory::instance()->setUntilMarkTextPointSize(value);
}

void DlgPrefWaveform::slotSetUntilMarkTextHeightLimit(int index) {
    WaveformWidgetFactory::instance()->setUntilMarkTextHeightLimit(
            WaveformWidgetFactory::toUntilMarkTextHeightLimit(index));
}

void DlgPrefWaveform::calculateCachedWaveformDiskUsage() {
    if (!m_pLibrary) {
        return; // no database (dialog built without a library, in tests)
    }
    AnalysisDao analysisDao(m_pConfig);
    QSqlDatabase dbConnection = mixxx::DbConnectionPooled(m_pLibrary->dbConnectionPool());
    size_t numBytes = analysisDao.getDiskUsageInBytes(dbConnection, AnalysisDao::TYPE_WAVEFORM) +
            analysisDao.getDiskUsageInBytes(dbConnection, AnalysisDao::TYPE_WAVESUMMARY);

    // Display total cached waveform size in mebibytes with 2 decimals.
    QString sizeMebibytes = QString::number(
            numBytes / (1024.0 * 1024.0), 'f', 2);

    waveformDiskUsage->setText(
            tr("Cached waveforms occupy %1 MiB on disk.").arg(sizeMebibytes));
}
