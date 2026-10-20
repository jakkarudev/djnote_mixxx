#include <gtest/gtest.h>

#include <QString>
#include <optional>
#include <ostream>

#include "widget/knobreadouttext.h"

namespace {

using namespace mixxx::knobreadout;

// The text uses a true minus sign (U+2212), not a hyphen.
const QString kMinus = QString(QChar(0x2212));
const QString kInfinity = QString(QChar(0x221E));
constexpr Mode kSimple = Mode::Simple;
constexpr Mode kPrecise = Mode::Precise;

struct Case {
    const char* name;
    const char* group;
    const char* item;
    double value;
    double parameter;
    QString expected;
    Mode mode = Mode::Simple;
    // Set for effect parameter knobs whose manifest is known.
    std::optional<EffectManifestParameter::UnitsHint> unitsHint = std::nullopt;
};

std::ostream& operator<<(std::ostream& os, const Case& c) {
    return os << c.name;
}

class KnobReadoutTextTest : public testing::TestWithParam<Case> {};

TEST_P(KnobReadoutTextTest, Text) {
    const Case& c = GetParam();
    const KnobIdentity knob{ConfigKey(c.group, c.item), c.unitsHint};
    Options options;
    options.mode = c.mode;
    EXPECT_EQ(c.expected.toStdString(),
            text(knob, KnobValue{c.value, c.parameter}, options).toStdString())
            << c.name;
}

// Gain factors: 10^(dB/20). Deck pregain spans 0..4 (+12.04 dB), main,
// headphone and booth gain span 0..5 (+13.98 dB).
INSTANTIATE_TEST_SUITE_P(Gains,
        KnobReadoutTextTest,
        testing::Values(
                Case{"deck gain fully down", "[Channel1]", "pregain", 0.0, 0.0, kMinus + kInfinity + " dB"},
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

// Deck EQ knobs: parameters of the deck's EQ-rack effect. Their value is the
// gain factor; the maximum is 4 (+12 dB) or 2 (+6 dB) depending on the EQ
// preferences, the knob centre is the neutral point either way.
INSTANTIATE_TEST_SUITE_P(DeckEq,
        KnobReadoutTextTest,
        testing::Values(
                Case{"simple: centred EQ", "[EqualizerRack1_[Channel1]_Effect1]", "parameter1", 1.0, 0.5, "0%", kSimple},
                Case{"simple: killed EQ", "[EqualizerRack1_[Channel1]_Effect1]", "parameter2", 0.0, 0.0, kMinus + "100%", kSimple},
                Case{"simple: EQ fully up", "[EqualizerRack1_[Channel2]_Effect1]", "parameter3", 4.0, 1.0, "+100%", kSimple},
                Case{"simple: EQ fully up on a 2x EQ", "[EqualizerRack1_[Channel2]_Effect1]", "parameter3", 2.0, 1.0, "+100%", kSimple},
                Case{"simple: EQ cut", "[EqualizerRack1_[Channel1]_Effect1]", "parameter1", 0.5, 0.25, kMinus + "50%", kSimple},
                Case{"simple: EQ boost rounds", "[EqualizerRack1_[Channel3]_Effect1]", "parameter1", 1.5, 0.6234, "+25%", kSimple},
                Case{"simple: a hair off centre has no sign", "[EqualizerRack1_[Channel1]_Effect1]", "parameter1", 1.0, 0.501, "0%", kSimple},
                Case{"precise: centred EQ", "[EqualizerRack1_[Channel1]_Effect1]", "parameter1", 1.0, 0.5, "0.0 dB", kPrecise},
                Case{"precise: killed EQ", "[EqualizerRack1_[Channel1]_Effect1]", "parameter2", 0.0, 0.0, kMinus + kInfinity + " dB", kPrecise},
                Case{"precise: 4x EQ fully up", "[EqualizerRack1_[Channel1]_Effect1]", "parameter3", 4.0, 1.0, "+12.0 dB", kPrecise},
                Case{"precise: 2x EQ fully up", "[EqualizerRack1_[Channel1]_Effect1]", "parameter3", 2.0, 1.0, "+6.0 dB", kPrecise},
                Case{"precise: EQ cut", "[EqualizerRack1_[Channel4]_Effect1]", "parameter1", 0.5, 0.25, kMinus + "6.0 dB", kPrecise}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

// Gains read in dB in both modes.
INSTANTIATE_TEST_SUITE_P(GainsInPreciseMode,
        KnobReadoutTextTest,
        testing::Values(
                Case{"deck gain max", "[Channel1]", "pregain", 4.0, 1.0, "+12.0 dB", kPrecise},
                Case{"main gain fully down", "[Master]", "gain", 0.0, 0.0, kMinus + kInfinity + " dB", kPrecise}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

// Main balance spans -1 (left) .. 1 (right); the headphone mix spans -1 (cue
// only) .. 1 (main only).
INSTANTIATE_TEST_SUITE_P(BalanceAndHeadphoneMix,
        KnobReadoutTextTest,
        testing::Values(
                Case{"balance centre", "[Master]", "balance", 0.0, 0.5, "C"},
                Case{"balance a hair off centre", "[Master]", "balance", 0.004, 0.502, "C"},
                Case{"balance left", "[Master]", "balance", -0.5, 0.25, "L 50%"},
                Case{"balance fully right", "[Main]", "balance", 1.0, 1.0, "R 100%"},
                Case{"balance in precise mode", "[Master]", "balance", 0.2, 0.6, "R 20%", kPrecise},
                Case{"headphone mix centre", "[Master]", "headMix", 0.0, 0.5, "Main 50% / Cue 50%"},
                Case{"headphone mix cue only", "[Master]", "headMix", -1.0, 0.0, "Main 0% / Cue 100%"},
                Case{"headphone mix mostly main", "[Main]", "headMix", 0.5, 0.75, "Main 75% / Cue 25%"}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

// The quick effect super knob and effect meta knobs read 0-100% in both modes.
INSTANTIATE_TEST_SUITE_P(SuperAndMetaKnobs,
        KnobReadoutTextTest,
        testing::Values(
                Case{"filter knob fully left", "[QuickEffectRack1_[Channel1]]", "super1", 0.0, 0.0, "0%"},
                Case{"filter knob centre in precise mode", "[QuickEffectRack1_[Channel2]]", "super1", 0.5, 0.5, "50%", kPrecise},
                Case{"meta knob", "[EffectRack1_EffectUnit1_Effect2]", "meta", 0.75, 0.75, "75%"},
                Case{"effect unit super knob", "[EffectRack1_EffectUnit2]", "super1", 1.0, 1.0, "100%", kPrecise}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

using Unit = EffectManifestParameter::UnitsHint;

// Other effect parameter knobs read their value with the manifest's units
// hint; "Unknown" falls back to the 0-100% knob position.
INSTANTIATE_TEST_SUITE_P(EffectParameters,
        KnobReadoutTextTest,
        testing::Values(
                Case{"unknown unit", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 0.3, 0.42, "42%", kSimple, Unit::Unknown},
                Case{"unknown unit in precise mode", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 0.3, 0.42, "42%", kPrecise, Unit::Unknown},
                Case{"echo delay in beats", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 0.25, 0.2, "0.25 beats", kSimple, Unit::Beats},
                Case{"whole beats", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 4.0, 1.0, "4 beats", kSimple, Unit::Beats},
                Case{"milliseconds", "[EffectRack1_EffectUnit2_Effect1]", "parameter2", 250.0, 0.5, "250 ms", kSimple, Unit::Millisecond},
                Case{"hertz", "[EffectRack1_EffectUnit1_Effect3]", "parameter1", 12.5, 0.1, "12.5 Hz", kSimple, Unit::Hertz},
                Case{"thousands of hertz read in kHz", "[EffectRack1_EffectUnit1_Effect3]", "parameter1", 1500.0, 0.6, "1.5 kHz", kSimple, Unit::Hertz},
                Case{"kilohertz", "[EffectRack1_EffectUnit1_Effect3]", "parameter1", 2.0, 0.6, "2 kHz", kSimple, Unit::KiloHertz},
                Case{"decibel cut", "[EffectRack1_EffectUnit1_Effect1]", "parameter3", -6.0, 0.3, kMinus + "6.0 dB", kSimple, Unit::Decibel},
                Case{"decibel boost", "[EffectRack1_EffectUnit1_Effect1]", "parameter3", 3.04, 0.7, "+3.0 dB", kPrecise, Unit::Decibel},
                Case{"cents", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", -50.0, 0.25, kMinus + "50 cents", kSimple, Unit::Cent},
                Case{"semitones", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 7.0, 0.8, "+7 st", kSimple, Unit::Semitone12tet},
                Case{"percentage", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 33.3, 0.333, "33.3%", kSimple, Unit::Percentage},
                Case{"coefficient has no unit", "[EffectRack1_EffectUnit1_Effect1]", "parameter2", 0.707107, 0.3, "0.707", kSimple, Unit::Coefficient},
                Case{"zero", "[EffectRack1_EffectUnit1_Effect1]", "parameter1", 0.0, 0.0, "0 ms", kSimple, Unit::Millisecond}),
        [](const testing::TestParamInfo<Case>& info) {
            return "case" + std::to_string(info.index);
        });

} // namespace
