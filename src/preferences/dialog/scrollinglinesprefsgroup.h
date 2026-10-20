#pragma once

#include <QGroupBox>
#include <QList>

#include "waveform/scrollinglinelayout.h"

class QCheckBox;
class QGridLayout;
class QSlider;
class QSpinBox;

// DJNote's "Beat, bar and step lines" group on the Waveforms preferences page:
// the master switch and the settings of the scrolling waveform's beat, bar and
// step lines. It owns its controls (built in code, not in the upstream .ui) and
// reports edits through settingsChanged(); the page passes them on to the
// waveform widget factory.
//
// While the master switch is on, upstream's beat grid opacity controls (passed
// in by the page) are disabled, never hidden or edited.
class ScrollingLinesPrefsGroup : public QGroupBox {
    Q_OBJECT
  public:
    ScrollingLinesPrefsGroup(const QList<QWidget*>& upstreamBeatGridControls,
            QWidget* pParent = nullptr);

    // Shows the settings in the controls, without emitting settingsChanged().
    void showSettings(const mixxx::scrollinglinelayout::LineSettings& settings);

  signals:
    void settingsChanged(const mixxx::scrollinglinelayout::LineSettings& settings);

  private:
    struct OpacityControls {
        QSlider* pSlider = nullptr;
        QSpinBox* pSpinBox = nullptr;
    };

    OpacityControls addOpacityRow(QGridLayout* pGrid,
            int& row,
            const QString& name,
            const QString& objectNamePrefix,
            const QString& explanation);
    void slotControlChanged();
    void updateUpstreamControls();

    QList<QWidget*> m_upstreamBeatGridControls;
    QCheckBox* m_pEnabledCheckBox = nullptr;
    OpacityControls m_barOpacity;
    OpacityControls m_beatOpacity;
    mixxx::scrollinglinelayout::LineSettings m_settings;
};
