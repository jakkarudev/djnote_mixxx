#include <gtest/gtest.h>

#include <QFont>
#include <QSizeF>
#include <QString>
#include <vector>

#include "widget/knobreadoutface.h"

// Fitting the compact readout text on a knob face (ticket #70). The fit is
// tested against a made-up text measure, so it doesn't depend on the fonts of
// the platform the tests run on.
namespace {

using namespace mixxx::knobreadout;

// A made-up font: each character 0.6 px wide and the line 1.2 px high per px
// of font size.
TextMeasure fakeMeasure(int characters) {
    return [characters](int pixelSize) {
        return QSizeF(characters * 0.6 * pixelSize, 1.2 * pixelSize);
    };
}

// E.g. "-12.0": with 2 px padding on each side, the label box at size s is
// (3s + 4) x 1.2s, whose diagonal is about 39.3 at 11, 36.1 at 10 and 26.4
// at 7.
const TextMeasure kFiveCharacters = fakeMeasure(5);

TEST(KnobReadoutFaceTest, FullSizeWhenTheLabelFitsAtTheLargestSize) {
    EXPECT_EQ(std::optional<int>(kFaceMaxPixelSize),
            faceFontPixelSize(kFiveCharacters, QSize(40, 40)));
}

TEST(KnobReadoutFaceTest, TheLargestSizeThatFitsIncludingThePadding) {
    // Without the padding, 11 would fit (diagonal about 35.5).
    EXPECT_EQ(std::optional<int>(10), faceFontPixelSize(kFiveCharacters, QSize(39, 39)));
}

TEST(KnobReadoutFaceTest, NoSizeBelowTheSmallest) {
    EXPECT_EQ(std::optional<int>(kFaceMinPixelSize),
            faceFontPixelSize(kFiveCharacters, QSize(27, 27)));
    EXPECT_FALSE(faceFontPixelSize(kFiveCharacters, QSize(26, 26)).has_value());
}

TEST(KnobReadoutFaceTest, ShorterTextFitsWhereLongerTextDoesNot) {
    EXPECT_TRUE(faceFontPixelSize(fakeMeasure(1), QSize(20, 20)).has_value());
    EXPECT_FALSE(faceFontPixelSize(fakeMeasure(6), QSize(20, 20)).has_value());
}

TEST(KnobReadoutFaceTest, TheFaceIsTheKnobsSmallerSide) {
    // A wide but short (or narrow but tall) knob widget has a small round face.
    EXPECT_EQ(std::optional<int>(10), faceFontPixelSize(kFiveCharacters, QSize(200, 39)));
    EXPECT_EQ(std::optional<int>(10), faceFontPixelSize(kFiveCharacters, QSize(39, 200)));
}

TEST(KnobReadoutFaceTest, MeasuresOnlySizesInTheRange) {
    std::vector<int> measured;
    faceFontPixelSize(
            [&measured](int pixelSize) {
                measured.push_back(pixelSize);
                return QSizeF(1000.0, 1000.0);
            },
            QSize(40, 40));
    EXPECT_EQ((std::vector<int>{11, 10, 9, 8, 7}), measured);
}

TEST(KnobReadoutFaceTest, RealFontFitsABigKnobButNotATinyOne) {
    // With the platform's font metrics: only checks the extremes.
    const QString text = QString(QChar(0x2212)) + QStringLiteral("12.0");
    EXPECT_EQ(std::optional<int>(kFaceMaxPixelSize),
            faceFontPixelSize(text, QFont(), QSize(200, 200)));
    EXPECT_FALSE(faceFontPixelSize(text, QFont(), QSize(4, 4)).has_value());
}

} // namespace
