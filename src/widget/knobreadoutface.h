#pragma once

#include <QFont>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <functional>
#include <optional>

// Fitting the knob adjustment readout (see GLOSSARY.md) on a knob face, for
// the knob-face style. No widget dependencies: only font metrics, so it is
// testable in isolation (knobreadoutfacetest.cpp), with a made-up text
// measure so the tests don't depend on the platform's fonts.
namespace mixxx {
namespace knobreadout {

// The readout's font size range on a knob face, in px.
constexpr int kFaceMaxPixelSize = 11;
constexpr int kFaceMinPixelSize = 7;
// Space between the text and the edge of its label on the face, in px.
constexpr int kFaceHorizontalPaddingPx = 2;
constexpr int kFaceVerticalPaddingPx = 0;

// The size of the readout text (without padding) at a font pixel size.
using TextMeasure = std::function<QSizeF(int pixelSize)>;

// The largest font pixel size, from kFaceMaxPixelSize down to
// kFaceMinPixelSize, at which the label of the text measured by measureText
// (padding included) fits inside the round face of a knob of knobSize: a
// circle as wide as the knob's smaller side. std::nullopt when it doesn't fit
// even at kFaceMinPixelSize; the caller then falls back to under the knob.
std::optional<int> faceFontPixelSize(const TextMeasure& measureText, QSize knobSize);

// The same for text in font, measured with its font metrics. font gives the
// family; its size is replaced.
std::optional<int> faceFontPixelSize(const QString& text, QFont font, QSize knobSize);

} // namespace knobreadout
} // namespace mixxx
