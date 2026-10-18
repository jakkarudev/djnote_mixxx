#include "waveform/widgets/allshader/phosphorwaveformwidget.h"

#include "waveform/renderers/allshader/waveformrenderbackground.h"
#include "waveform/renderers/allshader/waveformrenderbeat.h"
#include "waveform/renderers/allshader/waveformrendererendoftrack.h"
#include "waveform/renderers/allshader/waveformrendererpreroll.h"
#include "waveform/renderers/allshader/waveformrendererphosphor.h"
#include "waveform/renderers/allshader/waveformrendererslipmode.h"
#include "waveform/renderers/allshader/waveformrendermark.h"
#include "waveform/renderers/allshader/waveformrendermarkrange.h"
#include "waveform/widgets/allshader/moc_phosphorwaveformwidget.cpp"

namespace allshader {

PhosphorWaveformWidget::PhosphorWaveformWidget(const QString& group, QWidget* parent)
        : WaveformWidget(group, parent) {
    usePerBarZoom();
    // The light is added to RGB 24 whatever the skin's background is.
    addRenderer<WaveformRenderBackground>(QColor(24, 24, 24));
    addRenderer<WaveformRendererEndOfTrack>();
    addRenderer<WaveformRendererPreroll>();
    addRenderer<WaveformRenderMarkRange>();
    addRenderer<WaveformRendererPhosphor>();
    addRenderer<WaveformRenderBeat>();
    addRenderer<WaveformRenderMark>();
    // The following renderer will add an overlay waveform if a slip is in progress
    addRenderer<WaveformRendererSlipMode>();
    addRenderer<WaveformRendererPreroll>(::WaveformRendererAbstract::Slip);
    addRenderer<WaveformRendererPhosphor>(::WaveformRendererAbstract::Slip);
    addRenderer<WaveformRenderBeat>(::WaveformRendererAbstract::Slip);
    addRenderer<WaveformRenderMark>(::WaveformRendererAbstract::Slip);

    m_initSuccess = init();
}

void PhosphorWaveformWidget::castToQWidget() {
    m_widget = this;
}

void PhosphorWaveformWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
}

} // namespace allshader
