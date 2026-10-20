#include <gtest/gtest.h>

#include <QString>
#include <ostream>

#include "widget/knobreadouttext.h"

namespace {

using namespace mixxx::knobreadout;

// The text uses a true minus sign (U+2212), not a hyphen.
const QString kMinus = QString(QChar(0x2212));
const QString kInfinity = QString(QChar(0x221E));

struct Case {
    const char* name;
    const char* group;
    const char* item;
    double value;
    double parameter;
    QString expected;
};

std::ostream& operator<<(std::ostream& os, const Case& c) {
    return os << c.name;
}

class KnobReadoutTextTest : public testing::TestWithParam<Case> {};

TEST_P(KnobReadoutTextTest, Text) {
    const Case& c = GetParam();
    const KnobIdentity knob{ConfigKey(c.group, c.item)};
    EXPECT_EQ(c.expected.toStdString(),
            text(knob, KnobValue{c.value, c.parameter}).toStdString())
            << c.name;
}

// Gain factors: 10^(dB/20). Deck pregain spans 0..4 (+12.04 dB), main,
// headphone and booth gain span 0..5 (+13.98 dB).
INSTANTIATE_TEST_SUITE_P(Gains,
        KnobReadoutTextTest,
        testing::Values(
                Case{"deck gain fully down", "[Channel1]", "pregain", 0.0, 0.0,
                        kMinus + kInfinity + " dB"},
                Case{"deck gain centre", "[Channel1]", "pregain", 1.0, 0.5, "0.0 dB"},
                Case{"deck gain max", "[Channel2]", "pregain", 4.0, 1.0, "+12.0 dB"},
                Case{"deck gain cut", "[Channel1]", "pregain", 0.691831, 0.4, kMinus + "3.2 dB"},
                Case{"tiny cut rounds to zero, no sign",
                        "[Channel1]",
                        "pregain",
                        0.999,
                        0.5,
                        "0.0 dB"},
                Case{"main gain max", "[Master]", "gain", 5.0, 1.0, "+14.0 dB"},
                Case{"headphone gain max", "[Master]", "headGain", 5.0, 1.0, "+14.0 dB"},
                Case{"booth gain max", "[Master]", "booth_gain", 5.0, 1.0, "+14.0 dB"},
                Case{"main gain under the [Main] alias",
                        "[Main]",
                        "gain",
                        1.0,
                        0.5,
                        "0.0 dB"},
                Case{"sampler gain", "[Sampler3]", "pregain", 2.0, 0.75, "+6.0 dB"},
                Case{"microphone gain", "[Microphone2]", "pregain", 1.0, 0.5, "0.0 dB"},
                Case{"auxiliary gain fully down",
                        "[Auxiliary1]",
                        "pregain",
                        0.0,
                        0.0,
                        kMinus + kInfinity + " dB"}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

INSTANTIATE_TEST_SUITE_P(KnobPosition,
        KnobReadoutTextTest,
        testing::Values(
                Case{"filter knob centre", "[QuickEffectRack1_[Channel1]]", "super1", 0.5, 0.5, "50%"},
                Case{"knob fully left", "[EffectRack1_EffectUnit1]", "mix", 0.0, 0.0, "0%"},
                Case{"knob fully right", "[EffectRack1_EffectUnit1]", "mix", 1.0, 1.0, "100%"},
                Case{"knob position rounds", "[Channel1]", "rate", 0.0, 0.404, "40%"},
                Case{"gain item on a non-gain group",
                        "[Channel1]",
                        "gain",
                        0.3,
                        0.3,
                        "30%"}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

} // namespace
