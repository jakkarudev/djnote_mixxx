#pragma once

#include <future>
#include <memory>
#include <vector>

#include "shaders/phosphorshader.h"
#include "util/class.h"
#include "waveform/phosphor/phosphorlanecache.h"
#include "waveform/renderers/allshader/vertexdata.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererPhosphor;
}

/// The Phosphor signal layer. The lane model (CPU) turns the track's Phosphor
/// data into density columns; this layer only uploads the columns that
/// entered the view into a ring texture and lets the shader colour them.
class allshader::WaveformRendererPhosphor final : public allshader::WaveformRendererSignalBase {
  public:
    explicit WaveformRendererPhosphor(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play);
    ~WaveformRendererPhosphor() override;

    // override ::WaveformRendererSignalBase
    void onSetup(const QDomNode& node) override;
    void onSetTrack() override;

    void initializeGL() override;
    void paintGL() override;

  private:
    using Cache = mixxx::phosphor::PhosphorLaneCache;

    // The view the lane has to show now.
    mixxx::phosphor::LaneParams desiredParams(
            const mixxx::phosphor::PhosphorData& data) const;
    void resetCache();
    void updateCache(const mixxx::phosphor::PhosphorDataPointer& pData,
            const mixxx::phosphor::LaneParams& desired,
            int64_t firstColumn,
            int64_t lastColumn);
    void ensureTexture(int slotCount, int rows);
    void uploadColumns(int64_t firstColumn, int64_t lastColumn);
    void deleteTexture();
    void forgetUploadedColumns();

    mixxx::PhosphorShader m_shader;
    VertexData m_vertices;
    const bool m_isSlipRenderer;

    // The cache that is drawn, and the one a worker thread is building after
    // a zoom or size change.
    std::unique_ptr<Cache> m_pCache;
    std::future<std::unique_ptr<Cache>> m_pending;

    // Ring texture: one texel per lane row along x, one slot per column along
    // y, so that a run of columns is a contiguous block of the CPU copy.
    GLuint m_texture;
    int m_textureRows;
    int m_textureSlotCount;
    std::vector<uint8_t> m_texels;
    std::vector<int64_t> m_slotColumn; // which column a slot holds
    std::vector<uint8_t> m_slotDirty;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererPhosphor);
};
