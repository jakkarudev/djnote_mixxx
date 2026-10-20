#include <gtest/gtest.h>

#include <QFont>
#include <QString>

#include "widget/knobreadoutface.h"

// Fitting the compact readout text on a knob face (ticket #70).
namespace {

using namespace mixxx::knobreadout;

const QString kMinus = QString(QChar(0x2212));

QFont readoutFont() {
    QFont font;
    font.setPixelSize(kFaceMaxPixelSize);
    return font;
}

TEST(KnobReadoutFaceTest, LargeKnobShowsTheTextAtFullSize) {
    // E.g. the deck gain knob in LateNight.
    const auto size = faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(60, 60));
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(kFaceMaxPixelSize, *size);
}

TEST(KnobReadoutFaceTest, LateNightDeckGainKnobFitsTheLongestGainText) {
    // LateNight's deck gain knob is 40x34.
    EXPECT_TRUE(faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(40, 34)).has_value());
}

TEST(KnobReadoutFaceTest, SmallerKnobShrinksTheText) {
    const auto size = faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(30, 30));
    ASSERT_TRUE(size.has_value());
    EXPECT_LT(*size, kFaceMaxPixelSize);
    EXPECT_GE(*size, kFaceMinPixelSize);
}

TEST(KnobReadoutFaceTest, TinyKnobDoesNotFit) {
    EXPECT_FALSE(faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(12, 12)).has_value());
}

TEST(KnobReadoutFaceTest, ShorterTextFitsWhereLongerTextDoesNot) {
    EXPECT_TRUE(faceFontPixelSize(QStringLiteral("4"), readoutFont(), QSize(20, 20)).has_value());
    EXPECT_FALSE(faceFontPixelSize(kMinus + "100.0", readoutFont(), QSize(20, 20)).has_value());
}

TEST(KnobReadoutFaceTest, TheFaceIsTheKnobsSmallerSide) {
    // A wide but short knob widget has a small round face.
    EXPECT_EQ(faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(20, 20)),
            faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(200, 20)));
    EXPECT_FALSE(faceFontPixelSize(kMinus + "12.0", readoutFont(), QSize(200, 12)).has_value());
}

} // namespace
