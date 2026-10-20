#include <gtest/gtest.h>

#include <QString>
#include <ostream>

#include "widget/knobreadouttext.h"

// The compact form of the knob readout text, used by the knob-face style
// (ticket #70): no unit, shortest number form. Kept apart from
// knobreadouttexttest.cpp because it applies to every unit family.
namespace {

using namespace mixxx::knobreadout;

const QString kMinus = QString(QChar(0x2212));
const QString kInfinity = QString(QChar(0x221E));

struct CompactCase {
    const char* name;
    const char* group;
    const char* item;
    double value;
    double parameter;
    QString expected;
};

std::ostream& operator<<(std::ostream& os, const CompactCase& c) {
    return os << c.name;
}

class KnobReadoutCompactTextTest : public testing::TestWithParam<CompactCase> {};

TEST_P(KnobReadoutCompactTextTest, Text) {
    const CompactCase& c = GetParam();
    const KnobIdentity knob{ConfigKey(c.group, c.item)};
    Options options;
    options.compact = true;
    EXPECT_EQ(c.expected.toStdString(),
            text(knob, KnobValue{c.value, c.parameter}, options).toStdString())
            << c.name;
}

INSTANTIATE_TEST_SUITE_P(Compact,
        KnobReadoutCompactTextTest,
        testing::Values(
                CompactCase{"gain cut drops the unit",
                        "[Channel1]",
                        "pregain",
                        0.251189,
                        0.2,
                        kMinus + "12.0"},
                CompactCase{"gain boost drops the plus sign",
                        "[Channel1]",
                        "pregain",
                        4.0,
                        1.0,
                        "12.0"},
                CompactCase{"unity gain", "[Channel1]", "pregain", 1.0, 0.5, "0.0"},
                CompactCase{"gain fully down",
                        "[Channel1]",
                        "pregain",
                        0.0,
                        0.0,
                        kMinus + kInfinity},
                CompactCase{"knob position drops the percent sign",
                        "[EffectRack1_EffectUnit1]",
                        "mix",
                        0.4,
                        0.4,
                        "40"},
                CompactCase{"knob position fully right",
                        "[QuickEffectRack1_[Channel1]]",
                        "super1",
                        1.0,
                        1.0,
                        "100"}),
        [](const testing::TestParamInfo<CompactCase>& info) {
            return "case" + std::to_string(info.index);
        });

// The rule is generic, so it also covers units added later (EQ percent and
// dB, effect units, balance), whatever text the full form uses.
TEST(KnobReadoutCompactFormTest, DropsAnyTrailingUnit) {
    EXPECT_EQ("440", compactForm(QStringLiteral("440 Hz")).toStdString());
    EXPECT_EQ("2.5", compactForm(QStringLiteral("2.5 kHz")).toStdString());
    EXPECT_EQ("120", compactForm(QStringLiteral("120 ms")).toStdString());
    EXPECT_EQ("4", compactForm(QStringLiteral("4 beats")).toStdString());
    EXPECT_EQ(QString(kMinus + "100").toStdString(),
            compactForm(QString(kMinus + "100%")).toStdString());
    EXPECT_EQ("30", compactForm(QStringLiteral("+30%")).toStdString());
}

TEST(KnobReadoutCompactFormTest, KeepsALeadingLabelWithoutTheSpace) {
    EXPECT_EQ("L30", compactForm(QStringLiteral("L 30%")).toStdString());
    EXPECT_EQ("R5", compactForm(QStringLiteral("R 5%")).toStdString());
}

TEST(KnobReadoutCompactFormTest, LeavesTextWithoutANumberAlone) {
    EXPECT_EQ("C", compactForm(QStringLiteral("C")).toStdString());
}

} // namespace
