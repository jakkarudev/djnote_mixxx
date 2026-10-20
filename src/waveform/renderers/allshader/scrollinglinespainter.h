#pragma once

#include <QColor>
#include <QOpenGLFunctions>

#include "shaders/unicolorshader.h"
#include "waveform/renderers/allshader/vertexdata.h"

class WaveformWidgetRenderer;

namespace mixxx {
class Beats;
}

namespace allshader {

// DJNote: paints the scrolling waveform's beat, bar and step lines (see
// waveform/scrollinglinelayout.h) for upstream's WaveformRenderBeat, which
// calls this from paintGL() and passes in its GL functions, shader, vertex buffer and the
// skin's beat colour.
//
// Returns false without drawing anything while DJNote's lines are switched
// off (WaveformWidgetFactory::getScrollingLineSettings()); upstream's own
// drawing then runs unchanged.
bool paintScrollingLines(QOpenGLFunctions& gl,
        WaveformWidgetRenderer* pRenderer,
        bool isSlipRenderer,
        const mixxx::Beats& beats,
        mixxx::UnicolorShader& shader,
        VertexData& vertices,
        const QColor& skinColor);

} // namespace allshader
