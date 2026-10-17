#include "shaders/phosphorshader.h"

#include "waveform/phosphor/phosphortexelcodec.h"

using namespace mixxx;

void PhosphorShader::init() {
    // The texture has one texel per lane row along x and one column slot per
    // y; see phosphortexelcodec.h for the texel layout.
    QString vertexShaderCode = QStringLiteral(R"--(
uniform highp mat4 matrix;
attribute highp vec4 position; // lane pixels, x along the lane, y across (0 = top)
varying highp vec2 vLanePixel;
void main()
{
    vLanePixel = position.xy;
    gl_Position = matrix * position;
}
)--");

    QString fragmentShaderCode = QStringLiteral(R"--(
uniform sampler2D densityMap;
uniform highp vec2 mapSize;
uniform highp float ringOffset;
uniform highp vec3 color;
varying highp vec2 vLanePixel;
void main()
{
    highp float slot = mod(floor(vLanePixel.x) + ringOffset, mapSize.y);
    highp float row = min(floor(vLanePixel.y), mapSize.x - 1.0);
    highp vec4 texel = texture2D(densityMap,
            vec2((row + 0.5) / mapSize.x, (slot + 0.5) / mapSize.y));
    if (texel.g > 0.5) {
        // Digital silence: black, replacing what is behind.
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    highp float density = (texel.r * 256.0 + texel.b) * (255.0 / 65535.0) * DENSITY_MAX;
    // Alpha 0: the colour is added to what is behind.
    gl_FragColor = vec4(clamp(color * density, 0.0, 1.0), 0.0);
}
)--")
                                     .replace(QStringLiteral("DENSITY_MAX"),
                                             QString::number(phosphor::kTexelDensityMax, 'f', 3));

    load(vertexShaderCode, fragmentShaderCode);

    m_matrixLocation = uniformLocation("matrix");
    m_positionLocation = attributeLocation("position");
    m_textureLocation = uniformLocation("densityMap");
    m_mapSizeLocation = uniformLocation("mapSize");
    m_ringOffsetLocation = uniformLocation("ringOffset");
    m_colorLocation = uniformLocation("color");
}
