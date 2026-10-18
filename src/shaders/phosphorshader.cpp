#include "shaders/phosphorshader.h"

#include "waveform/phosphor/phosphorheight.h"
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
    // Output row y gets the integral of the source density over the source
    // interval [c + (y - c) / g, c + (y + 1 - c) / g] (stretchedRowDensity()
    // in phosphorheight.cpp does the same maths on the CPU).
    highp float halfLane = mapSize.x * 0.5;
    if (g < MIN_HEIGHT_FACTOR) {
        // Not drawn at all (no height, or less than 1/16 of the lane).
        gl_FragColor = vec4(0.0);
        return;
    }
    highp float low = max(0.0, halfLane + (floor(vLanePixel.y) - halfLane) / g);
    highp float high = min(mapSize.x, halfLane + (floor(vLanePixel.y) + 1.0 - halfLane) / g);
    highp float firstRow = floor(low);
    highp float density = 0.0;
    for (int k = 0; k < MAX_COVERED_ROWS; ++k) {
        highp float sourceRow = firstRow + float(k);
        if (sourceRow >= high || sourceRow >= mapSize.x) {
            break;
        }
        highp float overlap = min(high, sourceRow + 1.0) - max(low, sourceRow);
        if (overlap > 0.0) {
            texel = texture2D(densityMap,
                    vec2((sourceRow + 0.5) / mapSize.x, (slot + 0.5) / mapSize.y));
            highp float root = (texel.r * 256.0 + texel.b) * (255.0 / 65535.0);
            density += overlap * root * root * DENSITY_MAX;
        }
    }
    if (density <= 0.0) {
        // Not covered by the signal: nothing is drawn, whatever the blend, so
        // what is behind (background, marks) stays visible.
        gl_FragColor = vec4(0.0);
        return;
    }
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
                                                     QString::number(phosphor::kTexelDensityMax, 'f', 3))
                                     .replace(QStringLiteral("MIN_HEIGHT_FACTOR"),
                                                     QString::number(phosphor::kMinHeightFactor, 'f', 6))
                                     .replace(QStringLiteral("MAX_COVERED_ROWS"),
                                                     QString::number(phosphor::kMaxCoveredRows));

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
