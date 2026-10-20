#pragma once

#include <QGroupBox>
#include <QList>
#include <optional>

#include "waveform/scrollinglinelayout.h"

class QCheckBox;
class QColor;
class QColorDialog;
class QGridLayout;
class QPushButton;
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
    // A colour button that opens a colour dialog for a custom colour, and a
    // "Skin colour" button that goes back to the skin's beat colour.
    struct ColorControls {
        QPushButton* pColorButton = nullptr;
        QPushButton* pSkinColorButton = nullptr;
        QColorDialog* pDialog = nullptr;
    };
    using ColorMember = std::optional<QColor> mixxx::scrollinglinelayout::LineSettings::*;

    OpacityControls addOpacityRow(QGridLayout* pGrid,
            int& row,
            const QString& name,
            const QString& objectNamePrefix,
            const QString& explanation);
    QSpinBox* addWidthRow(QGridLayout* pGrid,
            int& row,
            const QString& name,
            const QString& objectNamePrefix,
            const QString& explanation);
    ColorControls addColorRow(QGridLayout* pGrid,
            int& row,
            const QString& name,
            const QString& objectNamePrefix,
            const QString& explanation,
            ColorMember pColor);
    ColorControls& colorControls(ColorMember pColor);
    void openColorDialog(ColorMember pColor);
    void setColor(ColorMember pColor, const std::optional<QColor>& color);
    static void showColor(const ColorControls& controls, const std::optional<QColor>& color);
    void slotControlChanged();
    void updateUpstreamControls();

    QList<QWidget*> m_upstreamBeatGridControls;
    QCheckBox* m_pEnabledCheckBox = nullptr;
    OpacityControls m_barOpacity;
    OpacityControls m_beatOpacity;
    QSpinBox* m_pBarWidthSpinBox = nullptr;
    QSpinBox* m_pBeatWidthSpinBox = nullptr;
    ColorControls m_barColor;
    ColorControls m_beatColor;
    mixxx::scrollinglinelayout::LineSettings m_settings;
};
