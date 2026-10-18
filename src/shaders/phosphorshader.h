#pragma once

#include "shaders/shader.h"

namespace mixxx {
class PhosphorShader;
}

/// Draws the Phosphor lane from the ring texture of density columns (see
/// phosphortexelcodec.h): band levels -> colour (saturation, brightness curve, channel gains) x density, added to what
/// is behind; digital silence is a black row.
class mixxx::PhosphorShader final : public mixxx::Shader {
  public:
    PhosphorShader() = default;
    ~PhosphorShader() = default;
    void init();

    int matrixLocation() const {
        return m_matrixLocation;
    }
    int positionLocation() const {
        return m_positionLocation;
    }
    int textureLocation() const {
        return m_textureLocation;
    }
    /// vec2: texture size in texels (rows per column, column slots)
    int mapSizeLocation() const {
        return m_mapSizeLocation;
    }
    /// float: ring slot of the lane's first column
    int ringOffsetLocation() const {
        return m_ringOffsetLocation;
    }
    /// sampler2D: the band levels of the column slots
    int colourTextureLocation() const {
        return m_colourTextureLocation;
    }
    /// vec3: the Low, Mid, High visual gains (applied to the band levels)
    int visualGainsLocation() const {
        return m_visualGainsLocation;
    }

  private:
    int m_matrixLocation;
    int m_positionLocation;
    int m_textureLocation;
    int m_mapSizeLocation;
    int m_ringOffsetLocation;
    int m_colourTextureLocation;
    int m_visualGainsLocation;

    DISALLOW_COPY_AND_ASSIGN(PhosphorShader)
};
