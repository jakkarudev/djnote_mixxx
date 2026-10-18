#include "shaders/phosphorshader.h"

#include "waveform/phosphor/phosphortexelcodec.h"

using namespace mixxx;

namespace {
// Ticket #14: the calibrated colour constants (research 8.3 / 8.5).
constexpr double kSaturation = 5.0;
constexpr double kLevelGamma = 0.87; // brightness follows level^0.87
constexpr double kBrightnessGamma = 0.35; // levels are peak^0.35
constexpr double kChannelGains[3] = {1.0, 0.85, 1.8};
constexpr double kBrightness = 0.7;
} // namespace

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
uniform sampler2D colourMap; // see phosphortexelcodec.h
uniform highp vec3 visualGains; // Low, Mid, High
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
    // Band levels of this column (R = Low, G = Mid, B = High).
    highp vec4 levelHigh = texture2D(colourMap, vec2((slot + 0.5) / mapSize.y, 0.25));
    highp vec4 levelLow = texture2D(colourMap, vec2((slot + 0.5) / mapSize.y, 0.75));
    highp vec3 levels = (levelHigh.rgb * 256.0 + levelLow.rgb) * (255.0 / 65535.0) * visualGains;
    highp float norm = length(levels);
    highp vec3 colour = vec3(0.0);
    if (norm > 1e-6) {
        // Raise the unit colour to the saturation and keep its length.
        highp vec3 hue = pow(levels / norm, vec3(SATURATION));
        hue /= max(length(hue), 1e-6);
        colour = hue * norm * pow(norm, BRIGHTNESS_EXPONENT);
    }
    colour *= vec3(CHANNEL_GAINS);
    // Alpha 0: the colour is added to what is behind.
    gl_FragColor = vec4(clamp(colour * (BRIGHTNESS * density), 0.0, 1.0), 0.0);
}
)--")
                                     .replace(QStringLiteral("DENSITY_MAX"),
                                                     QString::number(phosphor::kTexelDensityMax, 'f', 3))
                                     .replace(QStringLiteral("SATURATION"),
                                             QString::number(kSaturation, 'f', 3))
                                     .replace(QStringLiteral("BRIGHTNESS_EXPONENT"),
                                             QString::number(kLevelGamma / kBrightnessGamma - 1, 'f', 5))
                                     .replace(QStringLiteral("CHANNEL_GAINS"),
                                             QStringLiteral("%1, %2, %3")
                                                     .arg(kChannelGains[0], 0, 'f', 3)
                                                     .arg(kChannelGains[1], 0, 'f', 3)
                                                     .arg(kChannelGains[2], 0, 'f', 3))
                                     .replace(QStringLiteral("BRIGHTNESS"),
                                             QString::number(kBrightness, 'f', 3));

    load(vertexShaderCode, fragmentShaderCode);

    m_matrixLocation = uniformLocation("matrix");
    m_positionLocation = attributeLocation("position");
    m_textureLocation = uniformLocation("densityMap");
    m_mapSizeLocation = uniformLocation("mapSize");
    m_ringOffsetLocation = uniformLocation("ringOffset");
    m_colourTextureLocation = uniformLocation("colourMap");
    m_visualGainsLocation = uniformLocation("visualGains");
}
