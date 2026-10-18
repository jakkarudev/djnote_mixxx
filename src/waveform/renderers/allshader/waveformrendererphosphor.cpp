#include "waveform/renderers/allshader/waveformrendererphosphor.h"

#include <QOpenGLContext>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
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
} // namespace

WaveformRendererPhosphor::WaveformRendererPhosphor(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : WaveformRendererSignalBase(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_texture(0),
          m_colorTexture(0),
          m_textureRows(0),
          m_textureSlotCount(0) {
}

WaveformRendererPhosphor::~WaveformRendererPhosphor() {
    // The widget makes the GL context current before deleting its renderers.
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
    // Does not wait for a running build: its result is dropped.
    m_pending.cancel();
    m_pCache.reset();
    forgetUploadedColumns();
}

void WaveformRendererPhosphor::initializeGL() {
    WaveformRendererSignalBase::initializeGL();
    m_shader.init();
    // A new context: the texture is gone, upload everything again.
    m_texture = 0;
    m_colorTexture = 0;
    m_textureRows = 0;
    m_textureSlotCount = 0;
    m_slotColumn.clear();
}

void WaveformRendererPhosphor::deleteTexture() {
    if (m_texture != 0 && QOpenGLContext::currentContext()) {
        glDeleteTextures(1, &m_texture);
    }
    m_texture = 0;
    if (m_colorTexture != 0 && QOpenGLContext::currentContext()) {
        glDeleteTextures(1, &m_colorTexture);
    }
    m_colorTexture = 0;
}

LaneParams WaveformRendererPhosphor::desiredParams(const PhosphorData& data) const {
    const float dpr = m_waveformRenderer->getDevicePixelRatio();
    LaneParams params;
    const double framesPerLogicalPixel = m_waveformRenderer->getVisualSamplePerPixel() *
            data.sampleRate() / kVisualSamplesPerSecond;
    params.framesPerColumn = std::max(framesPerLogicalPixel / dpr, 1e-3);
    params.height = std::max(static_cast<int>(m_waveformRenderer->getBreadth() * dpr), 1);
    params.windowColumns = WaveformWidgetFactory::instance()->phosphorSettings().shapeWindow;
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
    if (auto pReady = m_pending.takeIfReady()) {
        m_pCache = std::move(pReady);
        forgetUploadedColumns();
    }

    if (m_pCache->params() == desired || m_pending.isRunning()) {
        // Up to date, or a recompute is running; the next frame asks again.
        return;
    }
    // Zoom or size changed: recompute the visible lane on a worker thread and
    // keep drawing the previous one meanwhile.
    m_pending.start([pData, desired, firstColumn, lastColumn]() {
        auto pCache = std::make_unique<Cache>(pData, desired);
        for (int64_t k = firstColumn; k <= lastColumn; ++k) {
            pCache->column(k);
        }
        return pCache;
    });
}

int WaveformRendererPhosphor::slotOf(int64_t column) const {
    return static_cast<int>(((column % m_textureSlotCount) + m_textureSlotCount) %
            m_textureSlotCount);
}

GLuint WaveformRendererPhosphor::createTexture(
        int width, int height, std::vector<uint8_t>* pTexels) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pTexels->assign(static_cast<size_t>(width) * height * kBytesPerTexel, 0);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RGBA,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pTexels->data());
    return texture;
}

void WaveformRendererPhosphor::ensureTexture(int slotCount, int rows) {
    if (m_texture != 0 && m_textureSlotCount == slotCount && m_textureRows == rows) {
        return;
    }
    deleteTexture();
    m_textureSlotCount = slotCount;
    m_textureRows = rows;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    m_texture = createTexture(rows, slotCount, &m_texels);
    m_colorTexture = createTexture(slotCount, kColorRows, &m_colorTexels);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_slotColumn.assign(static_cast<size_t>(slotCount), kNoColumn);
    m_slotDirty.assign(static_cast<size_t>(slotCount), 0);
}

void WaveformRendererPhosphor::uploadColumns(int64_t firstColumn, int64_t lastColumn) {
    const int slotCount = m_textureSlotCount;
    const int rows = m_textureRows;
    int computed = 0;
    for (int64_t k = firstColumn; k <= lastColumn; ++k) {
        const int slot = slotOf(k);
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
            uint8_t* pColorTexels = m_colorTexels.data();
            encodeColumnLevels(pColumn,
                    pColorTexels + static_cast<size_t>(slot) * kBytesPerTexel,
                    pColorTexels +
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
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        for (int colorRow = 0; colorRow < kColorRows; ++colorRow) {
            glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    slot,
                    colorRow,
                    end - slot,
                    1,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    m_colorTexels.data() +
                            (static_cast<size_t>(colorRow) * slotCount + slot) *
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
    const int ringOffset = slotOf(firstColumn);

    // The advanced settings: live, from the next frame on every deck.
    const PhosphorSettings& settings = WaveformWidgetFactory::instance()->phosphorSettings();
    glEnable(GL_BLEND);
    // Premultiplied: the color is added (alpha 0), silence replaces (alpha 1).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.enableAttributeArray(m_shader.positionLocation());
    m_shader.setUniformValue(m_shader.matrixLocation(), matrix);
    m_shader.setUniformValue(m_shader.textureLocation(), 0);
    m_shader.setUniformValue(m_shader.mapSizeLocation(),
            QVector2D(static_cast<float>(m_textureRows), static_cast<float>(m_textureSlotCount)));
    m_shader.setUniformValue(m_shader.ringOffsetLocation(), static_cast<float>(ringOffset));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glActiveTexture(GL_TEXTURE0);
    m_shader.setUniformValue(m_shader.colorTextureLocation(), 1);
    m_shader.setUniformValue(m_shader.visualGainsLocation(),
            QVector3D(gains.color[0], gains.color[1], gains.color[2]));
    m_shader.setUniformValue(m_shader.eqGainsLocation(),
            QVector3D(gains.eq[0], gains.eq[1], gains.eq[2]));
    m_shader.setUniformValue(m_shader.heightGainLocation(), gains.height);
    m_shader.setUniformValue(m_shader.saturationLocation(), static_cast<float>(settings.saturation));
    // Levels are peak^mix, the brightness follows peak^brightnessExponent.
    m_shader.setUniformValue(m_shader.brightnessExponentLocation(),
            static_cast<float>(settings.brightnessExponent / pData->colorMixExponent() - 1.0));
    m_shader.setUniformValue(m_shader.channelGainsLocation(),
            QVector3D(static_cast<float>(settings.channelGains[0]),
                    static_cast<float>(settings.channelGains[1]),
                    static_cast<float>(settings.channelGains[2])));
    m_shader.setUniformValue(m_shader.densityBrightnessLocation(),
            static_cast<float>(settings.densityBrightness));
    m_shader.setUniformValue(m_shader.opaqueLocation(),
            settings.blend == LightBlend::Opaque ? 1.f : 0.f);
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
