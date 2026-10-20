#pragma once

#include <QFont>
#include <QSize>
#include <QString>
#include <optional>

// Fitting the knob adjustment readout (see GLOSSARY.md) on a knob face, for
// the knob-face style. No widget dependencies: only font metrics, so it is
// testable in isolation (knobreadoutfacetest.cpp).
namespace mixxx {
namespace knobreadout {

// The readout's font size range on a knob face, in px.
constexpr int kFaceMaxPixelSize = 11;
constexpr int kFaceMinPixelSize = 7;
// Space between the text and the edge of its label on the face, in px.
constexpr int kFaceHorizontalPaddingPx = 2;
constexpr int kFaceVerticalPaddingPx = 0;

// The largest font pixel size, from kFaceMaxPixelSize down to
// kFaceMinPixelSize, at which a label of text (padding included) fits inside
// the round face of a knob of knobSize: a circle as wide as the knob's smaller
// side. std::nullopt when it doesn't fit even at kFaceMinPixelSize; the caller
// then falls back to under the knob. font gives the family; its size is
// replaced.
std::optional<int> faceFontPixelSize(const QString& text, QFont font, QSize knobSize);

} // namespace knobreadout
} // namespace mixxx
