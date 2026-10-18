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
uniform sampler2D colorMap; // see phosphortexelcodec.h
uniform highp vec3 visualGains; // Low, Mid, High: EQ x visual gains, for the color
uniform highp vec3 eqGains; // Low, Mid, High: knobs and kills only, for the height
uniform highp float heightGain; // All visual gain x total gain
// The advanced settings (phosphorsettings.h), live.
uniform highp float saturation;
uniform highp float brightnessExponent; // brightness / mix exponent - 1
uniform highp vec3 channelGains;
uniform highp float densityBrightness;
uniform highp float lightOpaque; // 0: the light is added, 1: it replaces what is behind
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
    // Band levels of this column (R = Low, G = Mid, B = High).
    highp vec4 levelHigh = texture2D(colorMap, vec2((slot + 0.5) / mapSize.y, 0.25));
    highp vec4 levelLow = texture2D(colorMap, vec2((slot + 0.5) / mapSize.y, 0.75));
    highp vec3 rawLevels = (levelHigh.rgb * 256.0 + levelLow.rgb) * (255.0 / 65535.0);
    // The column's height factor g: the gains stretch it about the lane center
    // (see phosphorheight.h, which tests the same maths).
    highp float rawNorm = length(rawLevels);
    highp float g = heightGain;
    if (rawNorm > 1e-6) {
        g *= length(rawLevels * eqGains) / rawNorm;
    }
    highp float halfLane = mapSize.x * 0.5;
    highp float sourceY = halfLane + (vLanePixel.y - halfLane) / max(g, 1e-6);
    if (g <= 1e-6 || sourceY < 0.0 || sourceY >= mapSize.x) {
        // Clipped at the lane edge, or no height at all.
        gl_FragColor = vec4(0.0, 0.0, 0.0, lightOpaque);
        return;
    }
    texel = texture2D(densityMap,
            vec2((floor(sourceY) + 0.5) / mapSize.x, (slot + 0.5) / mapSize.y));
    highp float density = (texel.r * 256.0 + texel.b) * (255.0 / 65535.0) * DENSITY_MAX / g;
    highp vec3 levels = rawLevels * visualGains;
    highp float norm = length(levels);
    highp vec3 color = vec3(0.0);
    if (norm > 1e-6) {
        // Raise the unit color to the saturation and keep its length.
        highp vec3 hue = pow(levels / norm, vec3(saturation));
        hue /= max(length(hue), 1e-6);
        color = hue * norm * pow(norm, brightnessExponent);
    }
    color *= channelGains;
    // Alpha 0: the color is added to what is behind (alpha 1: replaces it).
    gl_FragColor = vec4(clamp(color * (densityBrightness * density), 0.0, 1.0), lightOpaque);
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
    m_colorTextureLocation = uniformLocation("colorMap");
    m_visualGainsLocation = uniformLocation("visualGains");
    m_eqGainsLocation = uniformLocation("eqGains");
    m_heightGainLocation = uniformLocation("heightGain");
    m_saturationLocation = uniformLocation("saturation");
    m_brightnessExponentLocation = uniformLocation("brightnessExponent");
    m_channelGainsLocation = uniformLocation("channelGains");
    m_densityBrightnessLocation = uniformLocation("densityBrightness");
    m_opaqueLocation = uniformLocation("lightOpaque");
}
