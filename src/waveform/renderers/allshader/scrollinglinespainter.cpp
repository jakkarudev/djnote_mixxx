#include "waveform/renderers/allshader/scrollinglinespainter.h"

#include "track/beats.h"
#include "waveform/renderers/allshader/matrixforwidgetgeometry.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/scrollinglinelayout.h"
#include "waveform/waveformwidgetfactory.h"

namespace allshader {

bool paintScrollingLines(QOpenGLFunctions& gl,
        WaveformWidgetRenderer* pRenderer,
        bool isSlipRenderer,
        const mixxx::Beats& beats,
        mixxx::UnicolorShader& shader,
        VertexData& vertices,
        const QColor& skinColor) {
    const mixxx::scrollinglinelayout::LineSettings settings =
            WaveformWidgetFactory::instance()->getScrollingLineSettings();
    if (!settings.enabled) {
        return false;
    }

    const auto positionType = isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                             : ::WaveformRendererAbstract::Play;
    const double trackSamples = pRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return true;
    }
    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            pRenderer->getFirstDisplayedPosition(positionType) * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            pRenderer->getLastDisplayedPosition(positionType) * trackSamples);
    if (!startPosition.isValid() || !endPosition.isValid()) {
        return true;
    }

    // Lines on whole device pixels, as upstream draws them.
    const double devicePixelRatio = pRenderer->getDevicePixelRatio();
    const auto batches = mixxx::scrollinglinelayout::lineBatches(
            mixxx::scrollinglinelayout::layoutLines(&beats,
                    startPosition,
                    endPosition,
                    [&](mixxx::audio::FramePos position) {
                        const double x = pRenderer->transformSamplePositionInRendererWorld(
                                position.toEngineSamplePos(), positionType);
                        return qRound(x * devicePixelRatio) / devicePixelRatio;
                    },
                    settings,
                    pRenderer->getBeatGridAlpha(),
                    skinColor));
    if (batches.empty()) {
        return true;
    }

    const float rendererBreadth = pRenderer->getBreadth();
    const float lineBreadth = isSlipRenderer ? rendererBreadth / 2 : rendererBreadth;

    gl.glEnable(GL_BLEND);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int positionLocation = shader.positionLocation();
    const int matrixLocation = shader.matrixLocation();
    const int colorLocation = shader.colorLocation();

    shader.bind();
    shader.enableAttributeArray(positionLocation);
    shader.setUniformValue(matrixLocation, matrixForWidgetGeometry(pRenderer, false));

    for (const auto& batch : batches) {
        vertices.clear();
        vertices.reserve(static_cast<int>(batch.spans.size()) * 6); // 2 triangles each
        for (const auto& span : batch.spans) {
            vertices.addRectangle(static_cast<float>(span.left),
                    0.f,
                    static_cast<float>(span.right),
                    lineBreadth);
        }
        shader.setAttributeArray(positionLocation, GL_FLOAT, vertices.constData(), 2);
        shader.setUniformValue(colorLocation, batch.color);
        gl.glDrawArrays(GL_TRIANGLES, 0, vertices.size());
    }

    shader.disableAttributeArray(positionLocation);
    shader.release();
    return true;
}

} // namespace allshader
