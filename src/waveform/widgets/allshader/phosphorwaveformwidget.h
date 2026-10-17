#pragma once

#include "util/class.h"
#include "waveform/widgets/allshader/waveformwidget.h"

class WaveformWidgetFactory;

namespace allshader {
class PhosphorWaveformWidget;
}

class allshader::PhosphorWaveformWidget final : public allshader::WaveformWidget {
    Q_OBJECT
  public:
    WaveformWidgetType::Type getType() const override {
        return WaveformWidgetType::AllShaderPhosphor;
    }

    static inline QString getWaveformWidgetName() {
        return tr("Phosphor");
    }
    static constexpr bool useOpenGl() {
        return true;
    }
    static constexpr bool useOpenGles() {
        return true;
    }
    static constexpr bool useOpenGLShaders() {
        return true;
    }
    static constexpr bool useTextureForWaveform() {
        return false;
    }
    static constexpr WaveformWidgetCategory category() {
        return WaveformWidgetCategory::AllShader;
    }

  protected:
    void castToQWidget() override;
    void paintEvent(QPaintEvent* event) override;

  private:
    PhosphorWaveformWidget(const QString& group, QWidget* parent);
    friend class ::WaveformWidgetFactory;

    DISALLOW_COPY_AND_ASSIGN(PhosphorWaveformWidget);
};
