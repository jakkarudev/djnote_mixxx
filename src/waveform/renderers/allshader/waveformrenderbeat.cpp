#include "waveform/renderers/allshader/waveformrenderbeat.h"

#include <QDomNode>

#include "skin/legacy/skincontext.h"
#include "track/track.h"
#include "waveform/renderers/allshader/matrixforwidgetgeometry.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/scrollinglinelayout.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/wskincolor.h"

namespace allshader {

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : WaveformRenderer(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
}

void WaveformRenderBeat::initializeGL() {
    WaveformRenderer::initializeGL();
    m_shader.init();
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& context) {
    m_color = QColor(context.selectString(node, "BeatColor"));
    m_color = WSkinColor::getCorrectColor(m_color).toRgb();
}

void WaveformRenderBeat::paintGL() {
    TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();

    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return;
    }

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return;
    }

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition(positionType);
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition(positionType);

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);

    if (!startPosition.isValid() || !endPosition.isValid()) {
        return;
    }

    const float rendererBreadth = m_waveformRenderer->getBreadth();
    const float lineBreadth = m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth;

    // DJNote: the scrolling line layout decides which beat and bar lines to
    // draw and how. With its master switch off it returns upstream's lines.
    const auto lines = mixxx::scrollinglinelayout::layoutLines(trackBeats.get(),
            startPosition,
            endPosition,
            [&](mixxx::audio::FramePos position) {
                const double x = m_waveformRenderer->transformSamplePositionInRendererWorld(
                        position.toEngineSamplePos(), positionType);
                return qRound(x * devicePixelRatio) / static_cast<double>(devicePixelRatio);
            },
            WaveformWidgetFactory::instance()->getScrollingLineSettings(),
            m_waveformRenderer->getBeatGridAlpha(),
            m_color);
    if (lines.empty()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int positionLocation = m_shader.positionLocation();
    const int matrixLocation = m_shader.matrixLocation();
    const int colorLocation = m_shader.colorLocation();

    m_shader.bind();
    m_shader.enableAttributeArray(positionLocation);

    const QMatrix4x4 matrix = matrixForWidgetGeometry(m_waveformRenderer, false);
    m_shader.setUniformValue(matrixLocation, matrix);

    // One batch per line kind: all lines of a kind look the same (opacity,
    // width and colour). Wider lines grow both ways from the beat.
    for (const auto kind : {mixxx::scrollinglinelayout::LineKind::Bar,
                 mixxx::scrollinglinelayout::LineKind::Beat}) {
        m_vertices.clear();
        QColor color;
        for (const auto& line : lines) {
            if (line.kind == kind) {
                color = line.color;
                color.setAlphaF(line.opacityPercent / 100.0f);
                const float x1 = static_cast<float>(line.x) - (line.widthPx - 1) / 2;
                m_vertices.addRectangle(x1, 0.f, x1 + line.widthPx, lineBreadth);
            }
        }
        if (m_vertices.size() == 0) {
            continue;
        }
        m_shader.setAttributeArray(
                positionLocation, GL_FLOAT, m_vertices.constData(), 2);
        m_shader.setUniformValue(colorLocation, color);
        glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());
    }

    m_shader.disableAttributeArray(positionLocation);
    m_shader.release();
}

} // namespace allshader
