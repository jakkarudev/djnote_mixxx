#pragma once

#include <QColor>
#include <QList>
#include <QPixmap>

#include "analyzer/analyzerprogress.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "util/parented_ptr.h"
#include "waveform/phosphor/phosphoroverview.h"
#include "waveform/renderers/waveformmarkrange.h"
#include "waveform/renderers/waveformmarkset.h"
#include "waveform/renderers/waveformsignalcolors.h"
#include "waveform/waveform.h"
#include "widget/trackdroptarget.h"
#include "widget/wcuemenupopup.h"
#include "widget/wwidget.h"

class PlayerManager;
class QDomNode;
class SkinContext;

namespace mixxx {
class Beats;
}

namespace mixxx::overviewlayout {
struct CueTimesLayout;
struct HoverLineLayout;
struct HoverReadoutLayout;
struct Input;
} // namespace mixxx::overviewlayout

class WOverview : public WWidget, public TrackDropTarget {
    Q_OBJECT
  public:
    WOverview(
            const QString& group,
            PlayerManager* pPlayerManager,
            UserSettingsPointer pConfig,
            QWidget* parent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context);
    virtual void initWithTrack(TrackPointer pTrack);

    enum class Type {
        Filtered,
        HSV,
        RGB,
        Phosphor,
    };
    Q_ENUM(Type);

  public slots:
    void onConnectedControlChanged(double dParameter, double dValue) override;
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void onTrackAnalyzerProgress(TrackId trackId,
            AnalyzerProgress analyzerProgress);

  signals:
    void trackDropped(const QString& filename, const QString& group) override;
    void cloneDeck(const QString& sourceGroup, const QString& targetGroup) override;

  protected:

    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* /*unused*/) override;
    void resizeEvent(QResizeEvent* /*unused*/) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

  private slots:
    void onEndOfTrackChange(double v);

    void onMarkChanged(double v);
    void onMarkRangeChange(double v);
    void onRateRatioChange(double v);
    void onPassthroughChange(double v);
    void receiveCuesUpdated();

    void slotWaveformSummaryUpdated();
    void slotPhosphorOverviewUpdated();
    void slotCueMenuPopupAboutToHide();

    void slotTypeControlChanged(double v);
    void slotNormalizeOrVisualGainChanged();
    void slotPhosphorOverviewSettingsChanged();
    void slotHoverReadoutSettingsChanged();

  private:
    // Append the waveform overview pixmap according to available data
    // in waveform
    bool drawNextPixmapPart();
    // The Phosphor type: renders the stored columns built so far into the
    // source image through the pure image function.
    bool drawNextPhosphorPart();
    // How the Phosphor columns are drawn. The calibrated defaults; the settings
    // group (#55) loads them from the config.
    mixxx::phosphor::OverviewDrawSettings phosphorDrawSettings() const;
    void drawNextPixmapPartHSV(QPainter* pPainter,
            ConstWaveformPointer pWaveform,
            const int nextCompletion);
    void drawNextPixmapPartLMH(QPainter* pPainter,
            ConstWaveformPointer pWaveform,
            const int nextCompletion);
    void drawNextPixmapPartRGB(QPainter* pPainter,
            ConstWaveformPointer pWaveform,
            const int nextCompletion);

    void drawEndOfTrackBackground(QPainter* pPainter);
    void drawAxis(QPainter* pPainter);
    void drawBarAndPhraseLines(QPainter* pPainter);
    void drawWaveformPixmap(QPainter* pPainter);
    void drawPlayedOverlay(QPainter* pPainter);
    void drawPlayPosition(QPainter* pPainter);
    void drawEndOfTrackFrame(QPainter* pPainter);
    void drawAnalyzerProgress(QPainter* pPainter);
    void drawRangeMarks(QPainter* pPainter, const float& offset, const float& gain);
    // Where the line of a mark at the given sample position is drawn.
    float markLinePosition(double samplePosition, float offset, float gain) const;
    // The overview's state for the overview layout module: for the seek and
    // the time ruler, and with the hover readout and the cue times.
    mixxx::overviewlayout::Input makeSeekInput(const mixxx::Beats* pBeats);
    mixxx::overviewlayout::Input makeLayoutInput(const QFont& markerFont,
            const mixxx::Beats* pBeats,
            float offset,
            float gain);
    qreal hoverReadoutLabelWidth(const QFontMetricsF& fontMetrics, const QString& text) const;
    void drawMarks(QPainter* pPainter,
            const float offset,
            const float gain,
            const mixxx::overviewlayout::CueTimesLayout& cueTimesLayout);
    void drawPickupPosition(QPainter* pPainter, int pickupPos);
    void drawTimeRuler(QPainter* pPainter,
            const mixxx::overviewlayout::Input& layoutInput,
            const mixxx::overviewlayout::HoverReadoutLayout& readoutLayout);
    void drawHoverLine(QPainter* pPainter, double linePos);
    void drawPlainHover(QPainter* pPainter,
            const mixxx::overviewlayout::HoverLineLayout& lineLayout,
            const mixxx::overviewlayout::HoverReadoutLayout& readoutLayout,
            const QFont& markerFont);
    void drawHoverReadout(QPainter* pPainter,
            const mixxx::overviewlayout::HoverReadoutLayout& readoutLayout,
            const QPointF& anchor,
            const QFont& markerFont);
    void drawMarkLabels(QPainter* pPainter, const float offset, const float gain);
    void drawPassthroughOverlay(QPainter* pPainter);
    void paintText(const QString& text, QPainter* pPainter);
    double samplePositionToSeconds(double sample);
    inline int valueToPosition(double value) const {
        return static_cast<int>(m_a * value - m_b);
    }
    inline double positionToValue(int position) const {
        return (static_cast<double>(position) + m_b) / m_a;
    }

    void updateCues(const QList<CuePointer> &loadedCues);

    inline int length() {
        return m_orientation == Qt::Horizontal ? width() : height();
    }

    inline int breadth() {
        return m_orientation == Qt::Horizontal ? height() : width();
    }

    inline bool isPosInAllowedPosDragZone(const QPoint pos) {
        const QRect dragZone = rect().marginsAdded(QMargins(
                m_dragMarginH,
                m_dragMarginV,
                m_dragMarginH,
                m_dragMarginV));
        return dragZone.contains(pos);
    }

    ConstWaveformPointer getWaveform() const {
        return m_pWaveform;
    }

    double getTrackSamples() const {
        if (m_trackLoaded) {
            return m_trackSamplesControl.get();
        } else {
            // Ignore the value, because the engine can still have the old track
            // during loading
            return 0.0;
        }
    }

    // Hold the last visual sample processed to generate the pixmap

    const QString m_group;
    UserSettingsPointer m_pConfig;

    Type m_type;
    int m_actualCompletion;
    bool m_pixmapDone;
    float m_waveformPeak;
    float m_diffGain;
    qreal m_devicePixelRatio;
    bool m_endOfTrack;
    bool m_bPassthroughEnabled;

    parented_ptr<WCueMenuPopup> m_pCueMenuPopup;
    bool m_bShowCueTimes;

    int m_iPosSeconds;
    // True if pick-up is dragged. Only used when m_bEventWhileDrag is false
    bool m_bLeftClickDragging;
    // Internal storage of slider position in pixels
    int m_iPickupPos;
    // position of the overlay shadow
    int m_iPlayPos;
    bool m_bTimeRulerActive;
    // Pointer is over the widget (plain hover readout), at m_hoverPos.
    bool m_bPointerInside;
    QPointF m_hoverPos;
    Qt::Orientation m_orientation;
    int m_dragMarginH;
    int m_dragMarginV;
    int m_iLabelFontSize;

    // Coefficient value-position linear transposition
    double m_a;
    double m_b;

    AnalyzerProgress m_analyzerProgress;
    bool m_trackLoaded;
    WaveformMarkPointer m_pHoveredMark;
    double m_scaleFactor;

    // Current active track
    TrackPointer m_pCurrentTrack;
    ConstWaveformPointer m_pWaveform;
    mixxx::phosphor::PhosphorOverviewPointer m_pPhosphorOverview;

    QImage m_waveformSourceImage;
    QImage m_waveformImageScaled;

    WaveformSignalColors m_signalColors;

    parented_ptr<ControlProxy> m_endOfTrackControl;
    parented_ptr<ControlProxy> m_pRateRatioControl;
    PollingControlProxy m_trackSampleRateControl;
    PollingControlProxy m_trackSamplesControl;
    PollingControlProxy m_playpositionControl;
    parented_ptr<ControlProxy> m_pSnappedSeekControl;
    parented_ptr<ControlProxy> m_pPassthroughControl;
    parented_ptr<ControlProxy> m_pTypeControl;

    QPointF m_timeRulerPos;
    WaveformMarkLabel m_timeRulerPositionLabel;
    WaveformMarkLabel m_timeRulerDistanceLabel;
    // Hover readout (own row; cleared if merged into
    // m_timeRulerDistanceLabel or if there is no beat grid).
    WaveformMarkLabel m_hoverReadoutLabel;

    QPixmap m_backgroundPixmap;
    QString m_backgroundPixmapPath;
    QColor m_backgroundColor;
    QColor m_labelTextColor;
    QColor m_labelBackgroundColor;
    QColor m_axesColor;
    QColor m_playPosColor;
    QColor m_barLineColor;
    QColor m_phraseLineColor;
    QColor m_endOfTrackColor;
    QColor m_passthroughOverlayColor;
    QColor m_playedOverlayColor;
    QColor m_lowColor;
    int m_dimBrightThreshold;
    parented_ptr<QLabel> m_pPassthroughLabel;

    WaveformMarkSet m_marks;
    std::vector<WaveformMarkRange> m_markRanges;
    WaveformMarkLabel m_cuePositionLabel;
    WaveformMarkLabel m_cueTimeDistanceLabel;
};
