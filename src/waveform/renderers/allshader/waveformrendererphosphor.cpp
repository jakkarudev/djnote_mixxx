#include "waveform/renderers/allshader/waveformrendererphosphor.h"

#include <QOpenGLContext>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <chrono>
#include <cmath>

#include "track/track.h"
#include "waveform/phosphor/phosphorheight.h"
#include "waveform/phosphor/phosphortexelcodec.h"
#include "waveform/renderers/allshader/matrixforwidgetgeometry.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveformwidgetfactory.h"

namespace allshader {

using namespace mixxx::phosphor;

namespace {
// Columns computed on the GUI thread per frame; the rest of a big change is
// filled in over the next frames (a zoom or size change is done on a worker).
constexpr int kMaxColumnsPerFrame = 64;
constexpr int64_t kNoColumn = INT64_MIN;
// Ticket #12: Mixxx's existing zoom mapping. The deck's zoom is in visual
// samples (441 per second at the file's rate) per logical pixel.
constexpr double kVisualSamplesPerSecond = 441.0;
constexpr double kWindowColumns = 2.0;
} // namespace

WaveformRendererPhosphor::WaveformRendererPhosphor(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : WaveformRendererSignalBase(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_texture(0),
          m_colourTexture(0),
          m_textureRows(0),
          m_textureSlotCount(0) {
}

WaveformRendererPhosphor::~WaveformRendererPhosphor() {
    // The widget makes the GL context current before deleting its renderers.
    if (m_pending.valid()) {
        m_pending.wait();
    }
    deleteTexture();
}

void WaveformRendererPhosphor::onSetup(const QDomNode& node) {
    Q_UNUSED(node);
}

void WaveformRendererPhosphor::onSetTrack() {
    resetCache();
}

void WaveformRendererPhosphor::forgetUploadedColumns() {
    std::fill(m_slotColumn.begin(), m_slotColumn.end(), kNoColumn);
}

void WaveformRendererPhosphor::resetCache() {
    if (m_pending.valid()) {
        m_pending.wait();
        m_pending = {};
    }
    m_pCache.reset();
    forgetUploadedColumns();
}

void WaveformRendererPhosphor::initializeGL() {
    WaveformRendererSignalBase::initializeGL();
    m_shader.init();
    // A new context: the texture is gone, upload everything again.
    m_texture = 0;
    m_colourTexture = 0;
    m_textureRows = 0;
    m_textureSlotCount = 0;
    m_slotColumn.clear();
}

void WaveformRendererPhosphor::deleteTexture() {
    if (m_texture != 0 && QOpenGLContext::currentContext()) {
        glDeleteTextures(1, &m_texture);
    }
    m_texture = 0;
    if (m_colourTexture != 0 && QOpenGLContext::currentContext()) {
        glDeleteTextures(1, &m_colourTexture);
    }
    m_colourTexture = 0;
}

LaneParams WaveformRendererPhosphor::desiredParams(const PhosphorData& data) const {
    const float dpr = m_waveformRenderer->getDevicePixelRatio();
    LaneParams params;
    const double framesPerLogicalPixel = m_waveformRenderer->getVisualSamplePerPixel() *
            data.sampleRate() / kVisualSamplesPerSecond;
    params.framesPerColumn = std::max(framesPerLogicalPixel / dpr, 1e-3);
    params.height = std::max(static_cast<int>(m_waveformRenderer->getBreadth() * dpr), 1);
    params.windowColumns = kWindowColumns;
    // The slip overlay is the full shape at half scale in the top half.
    return m_isSlipRenderer ? slipOverlayParams(params) : params;
}

void WaveformRendererPhosphor::updateCache(const PhosphorDataPointer& pData,
        const LaneParams& desired,
        int64_t firstColumn,
        int64_t lastColumn) {
    if (!m_pCache || m_pCache->data() != pData) {
        resetCache();
        m_pCache = std::make_unique<Cache>(pData, desired);
        return;
    }

    // Collect a finished recompute.
    if (m_pending.valid() &&
            m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_pCache = m_pending.get();
        m_pending = {};
        forgetUploadedColumns();
    }

    if (m_pCache->params() == desired || m_pending.valid()) {
        // Up to date, or a recompute is running; the next frame asks again.
        return;
    }
    // Zoom or size changed: recompute the visible lane on a worker thread and
    // keep drawing the previous one meanwhile.
    m_pending = std::async(std::launch::async, [pData, desired, firstColumn, lastColumn]() {
        auto pCache = std::make_unique<Cache>(pData, desired);
        for (int64_t k = firstColumn; k <= lastColumn; ++k) {
            pCache->column(k);
        }
        return pCache;
    });
}

void WaveformRendererPhosphor::ensureTexture(int slotCount, int rows) {
    if (m_texture != 0 && m_textureSlotCount == slotCount && m_textureRows == rows) {
        return;
    }
    deleteTexture();
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_textureSlotCount = slotCount;
    m_textureRows = rows;
    m_texels.assign(static_cast<size_t>(slotCount) * rows * kBytesPerTexel, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RGBA,
            rows,
            slotCount,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            m_texels.data());
    glGenTextures(1, &m_colourTexture);
    glBindTexture(GL_TEXTURE_2D, m_colourTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_colourTexels.assign(static_cast<size_t>(slotCount) * kColourRows * kBytesPerTexel, 0);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RGBA,
            slotCount,
            kColourRows,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            m_colourTexels.data());
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_slotColumn.assign(static_cast<size_t>(slotCount), kNoColumn);
    m_slotDirty.assign(static_cast<size_t>(slotCount), 0);
}

void WaveformRendererPhosphor::uploadColumns(int64_t firstColumn, int64_t lastColumn) {
    const int slotCount = m_textureSlotCount;
    const int rows = m_textureRows;
    int computed = 0;
    for (int64_t k = firstColumn; k <= lastColumn; ++k) {
        const int slot = static_cast<int>(((k % slotCount) + slotCount) % slotCount);
        if (m_slotColumn[static_cast<size_t>(slot)] == k) {
            continue;
        }
        if (!m_pCache->contains(k)) {
            if (computed >= kMaxColumnsPerFrame) {
                continue; // later frames
            }
            ++computed;
        }
        const LaneColumn* pColumn = m_pCache->column(k);
        encodeColumn(pColumn,
                rows,
                m_texels.data() + static_cast<size_t>(slot) * rows * kBytesPerTexel);
        // A column that can't be computed yet (data still decoding) is blank
        // and asked for again.
        {
            uint8_t* pColourTexels = m_colourTexels.data();
            encodeColumnLevels(pColumn,
                    pColourTexels + static_cast<size_t>(slot) * kBytesPerTexel,
                    pColourTexels +
                            (static_cast<size_t>(slotCount) + static_cast<size_t>(slot)) *
                                    kBytesPerTexel);
        }
        m_slotColumn[static_cast<size_t>(slot)] = pColumn ? k : kNoColumn;
        m_slotDirty[static_cast<size_t>(slot)] = 1;
    }
    // Upload runs of consecutive dirty slotCount.
    int slot = 0;
    while (slot < slotCount) {
        if (!m_slotDirty[static_cast<size_t>(slot)]) {
            ++slot;
            continue;
        }
        int end = slot;
        while (end < slotCount && m_slotDirty[static_cast<size_t>(end)]) {
            m_slotDirty[static_cast<size_t>(end)] = 0;
            ++end;
        }
        glTexSubImage2D(GL_TEXTURE_2D,
                0,
                0,
                slot,
                rows,
                end - slot,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                m_texels.data() + static_cast<size_t>(slot) * rows * kBytesPerTexel);
        // The levels of the same run, in the second texture.
        glBindTexture(GL_TEXTURE_2D, m_colourTexture);
        for (int colourRow = 0; colourRow < kColourRows; ++colourRow) {
            glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    slot,
                    colourRow,
                    end - slot,
                    1,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    m_colourTexels.data() +
                            (static_cast<size_t>(colourRow) * slotCount + slot) *
                                    kBytesPerTexel);
        }
        glBindTexture(GL_TEXTURE_2D, m_texture);
        slot = end;
    }
}

void WaveformRendererPhosphor::paintGL() {
    if (m_isSlipRenderer && !m_waveformRenderer->isSlipActive()) {
        return;
    }
    const TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack) {
        return;
    }
    const PhosphorDataPointer pData = pTrack->getPhosphorData();
    if (!pData) {
        // Samplers, preview decks and tracks not analyzed yet: empty signal
        // layer, the overlays keep working.
        return;
    }

    const float dpr = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength() * dpr);
    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (length <= 0 || trackSamples <= 0 || pData->totalFrames() <= 0) {
        return;
    }

    // Gains (visual, total, EQ) only reach the shader: they stretch the
    // columns there and never touch the density cache.
    const ShaderGains gains = shaderGains(gainInputs(false));

    const LaneParams desired = desiredParams(*pData);
    // The play position as a fraction of the track keeps working when the
    // file's rate differs from the engine's.
    const double positionFrame =
            m_waveformRenderer->getTruePosSample(m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                                  : ::WaveformRendererAbstract::Play) /
            trackSamples * pData->totalFrames();
    const double playMarkerX = m_waveformRenderer->getPlayMarkerPosition() * length;

    // Where a recompute has to look: the same lane in the new grid.
    const int64_t wantFirst = std::llround(positionFrame / desired.framesPerColumn - playMarkerX);
    updateCache(pData, desired, wantFirst, wantFirst + length - 1);

    // Draw with the grid of the cache that is ready (the old zoom while the
    // recompute runs).
    const LaneParams drawn = m_pCache->params();
    const int64_t firstColumn = std::llround(positionFrame / drawn.framesPerColumn - playMarkerX);
    const int64_t lastColumn = firstColumn + length - 1;
    m_pCache->retain(firstColumn, lastColumn);

    ensureTexture(std::max(length, m_textureSlotCount), drawn.height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    uploadColumns(firstColumn, lastColumn);

    // The slip overlay covers the top half of the lane (its cache is
    // half as tall, one texel row per pixel).
    float breadth = static_cast<float>(m_waveformRenderer->getBreadth()) * dpr;
    if (m_isSlipRenderer) {
        breadth = static_cast<float>(drawn.height);
    }
    m_vertices.clear();
    m_vertices.addRectangle(0.f, 0.f, static_cast<float>(length), breadth);

    const QMatrix4x4 matrix = matrixForWidgetGeometry(m_waveformRenderer, true);
    const int ringOffset = static_cast<int>(
            ((firstColumn % m_textureSlotCount) + m_textureSlotCount) % m_textureSlotCount);

    glEnable(GL_BLEND);
    // Premultiplied: the colour is added (alpha 0), silence replaces (alpha 1).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.enableAttributeArray(m_shader.positionLocation());
    m_shader.setUniformValue(m_shader.matrixLocation(), matrix);
    m_shader.setUniformValue(m_shader.textureLocation(), 0);
    m_shader.setUniformValue(m_shader.mapSizeLocation(),
            QVector2D(static_cast<float>(m_textureRows), static_cast<float>(m_textureSlotCount)));
    m_shader.setUniformValue(m_shader.ringOffsetLocation(), static_cast<float>(ringOffset));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_colourTexture);
    glActiveTexture(GL_TEXTURE0);
    m_shader.setUniformValue(m_shader.colourTextureLocation(), 1);
    m_shader.setUniformValue(m_shader.visualGainsLocation(),
            QVector3D(gains.colour[0], gains.colour[1], gains.colour[2]));
    m_shader.setUniformValue(m_shader.eqGainsLocation(),
            QVector3D(gains.eq[0], gains.eq[1], gains.eq[2]));
    m_shader.setUniformValue(m_shader.heightGainLocation(), gains.height);
    m_shader.setAttributeArray(m_shader.positionLocation(), GL_FLOAT, m_vertices.constData(), 2);

    glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());

    m_shader.disableAttributeArray(m_shader.positionLocation());
    m_shader.release();
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace allshader
