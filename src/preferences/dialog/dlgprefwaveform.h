#pragma once

#include <memory>

#include "preferences/dialog/dlgpreferencepage.h"
#include "waveform/phosphor/phosphorsettings.h"
#include "preferences/dialog/ui_dlgprefwaveformdlg.h"
#include "preferences/usersettings.h"

class ControlPushButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QWidget;
class Library;

namespace mixxx::overviewlayout {
struct HoverReadoutSettings;
}

class DlgPrefWaveform : public DlgPreferencePage, public Ui::DlgPrefWaveformDlg {
    Q_OBJECT
  public:
    DlgPrefWaveform(
            QWidget* pParent,
            UserSettingsPointer pConfig,
            std::shared_ptr<Library> pLibrary);
    virtual ~DlgPrefWaveform();

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;
    void slotSetWaveformEndRender(int endTime);

  private slots:
    void slotSetFrameRate(int frameRate);
    void slotSetWaveformType(int index);
    void slotSetWaveformOverviewType();
    void slotSetDefaultZoom(int index);
    void slotSetZoomSynchronization(bool checked);
    void slotSetVisualGainAll(double gain);
    void slotSetVisualGainLow(double gain);
    void slotSetVisualGainMid(double gain);
    void slotSetVisualGainHigh(double gain);
    void slotSetFollowsGain(bool follows);
    void slotSetFollowsEq(bool follows);
    void slotSetNormalizeOverview(bool normalize);
    void slotSetPhosphorDetail(int index);
    void slotPhosphorAdvancedChanged();
    void slotResetPhosphorAdvanced();
    void slotPhosphorOverviewChanged();
    void slotResetPhosphorOverview();
    void slotSetOverviewLinesMode(int index);
    void slotSetOverviewPhraseBars(int index);
    void slotSetOverviewLineOpacity(int percent);
    void slotWaveformMeasured(float frameRate, int droppedFrames);
    void slotClearCachedWaveforms();
    void slotSetBeatGridAlpha(int alpha);
    void slotSetPlayMarkerPosition(int position);
    void slotSetUntilMarkShowBeats(bool checked);
    void slotSetUntilMarkShowTime(bool checked);
    void slotSetUntilMarkAlign(int index);
    void slotSetUntilMarkTextPointSize(int value);
    void slotSetUntilMarkTextHeightLimit(int index);
    void slotSetOverviewHoverReadout();

  private:
    void initWaveformControl();
    void buildPhosphorAdvanced();
    void showPhosphorSettings(const mixxx::phosphor::PhosphorSettings& settings);
    void buildPhosphorOverviewAdvanced();
    void showPhosphorOverviewSettings(const mixxx::phosphor::PhosphorOverviewSettings& settings);
    void showHoverReadoutControls(const mixxx::overviewlayout::HoverReadoutSettings& settings);
    void calculateCachedWaveformDiskUsage();
    void notifyRebootNecessary();
    void updateEnableUntilMark();

    std::unique_ptr<ControlPushButton> m_pTypeControl;

    // Phosphor advanced settings (built in code, see buildPhosphorAdvanced).
    QCheckBox* m_pPhosphorAdvancedCheckBox = nullptr;
    QWidget* m_pPhosphorAdvancedWidget = nullptr;
    QDoubleSpinBox* m_pPhosphorSaturation = nullptr;
    QDoubleSpinBox* m_pPhosphorGain[3] = {};
    QDoubleSpinBox* m_pPhosphorBrightnessExponent = nullptr;
    QDoubleSpinBox* m_pPhosphorDensityBrightness = nullptr;
    QComboBox* m_pPhosphorBlend = nullptr;
    QDoubleSpinBox* m_pPhosphorShapeWindow = nullptr;
    QDoubleSpinBox* m_pPhosphorMixExponent = nullptr;
    QDoubleSpinBox* m_pPhosphorAttack = nullptr;
    QDoubleSpinBox* m_pPhosphorRelease[3] = {};
    QDoubleSpinBox* m_pPhosphorLookAhead = nullptr;
    QPushButton* m_pPhosphorResetButton = nullptr;
    // Phosphor overview advanced settings (see buildPhosphorOverviewAdvanced).
    QWidget* m_pPhosphorOverviewWidget = nullptr;
    QDoubleSpinBox* m_pOverviewBrightness = nullptr;
    QDoubleSpinBox* m_pOverviewLevelExponent = nullptr;
    QDoubleSpinBox* m_pOverviewDensityExponent = nullptr;
    QDoubleSpinBox* m_pOverviewGain[3] = {};
    QPushButton* m_pOverviewResetButton = nullptr;

    UserSettingsPointer m_pConfig;
    std::shared_ptr<Library> m_pLibrary;
};
