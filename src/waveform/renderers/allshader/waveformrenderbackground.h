#pragma once

#include <QColor>
#include <optional>

#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderer.h"

namespace allshader {
class WaveformRenderBackground;
}

class allshader::WaveformRenderBackground final : public allshader::WaveformRenderer {
  public:
    /// fixedColor replaces the skin's background color when given.
    explicit WaveformRenderBackground(WaveformWidgetRenderer* waveformWidgetRenderer,
            std::optional<QColor> fixedColor = std::nullopt);

    void setup(const QDomNode& node, const SkinContext& context) override;
    void paintGL() override;

  private:
    const std::optional<QColor> m_fixedColor;
    QColor m_backgroundColor;

    DISALLOW_COPY_AND_ASSIGN(WaveformRenderBackground);
};
