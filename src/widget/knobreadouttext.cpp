#include "widget/knobreadouttext.h"

#include <algorithm>
#include <cmath>

namespace mixxx {
namespace knobreadout {

namespace {

const QString kMinus = QString(QChar(0x2212));
const QString kMinusInfinityDb = QString(QChar(0x2212)) + QChar(0x221E) + QStringLiteral(" dB");

// The main mixer's group; "[Main]" is its newer alias.
bool isMainGroup(const QString& group) {
    return group == QStringLiteral("[Master]") || group == QStringLiteral("[Main]");
}

bool isAudioTaperGain(const ConfigKey& key) {
    if (key.item == QStringLiteral("pregain")) {
        // Deck, preview deck, sampler, microphone and auxiliary channels.
        return true;
    }
    return isMainGroup(key.group) &&
            (key.item == QStringLiteral("gain") ||
                    key.item == QStringLiteral("headGain") ||
                    key.item == QStringLiteral("booth_gain"));
}

// One decimal place, a true minus sign, "+" above 0 dB and no sign on a value
// that rounds to 0.0.
QString decibelText(double factor) {
    if (!(factor > 0.0)) {
        return kMinusInfinityDb;
    }
    const double tenths = std::round(20.0 * std::log10(factor) * 10.0);
    if (tenths == 0.0) {
        return QStringLiteral("0.0 dB");
    }
    const QString magnitude = QString::number(std::abs(tenths) / 10.0, 'f', 1);
    return (tenths < 0.0 ? kMinus : QStringLiteral("+")) + magnitude +
            QStringLiteral(" dB");
}

// The knob parameters of a deck's EQ-rack effect, e.g.
// [EqualizerRack1_[Channel1]_Effect1],parameter1. Their manifests say
// "Unknown" units, so the group tells them apart.
bool isDeckEq(const ConfigKey& key) {
    return key.group.startsWith(QStringLiteral("[EqualizerRack")) &&
            key.item.startsWith(QStringLiteral("parameter"));
}

// A signed whole percent with a true minus sign, "+" above 0 and no sign on
// a value that rounds to 0.
QString signedPercentText(double percent) {
    const double rounded = std::round(percent);
    if (rounded == 0.0) {
        return QStringLiteral("0%");
    }
    return (rounded < 0.0 ? kMinus : QStringLiteral("+")) +
            QString::number(static_cast<int>(std::abs(rounded))) + QStringLiteral("%");
}

// The knob's travel from its neutral centre: -100% fully left, +100% fully
// right, whatever the EQ maximum.
QString neutralCentrePercentText(double parameter) {
    return signedPercentText((std::clamp(parameter, 0.0, 1.0) - 0.5) * 200.0);
}

// The main mixer's balance: -1 (left) .. 1 (right).
QString balanceText(double value) {
    const int percent = static_cast<int>(std::round(std::clamp(value, -1.0, 1.0) * 100.0));
    if (percent == 0) {
        return QStringLiteral("C");
    }
    return (percent < 0 ? QStringLiteral("L ") : QStringLiteral("R ")) +
            QString::number(std::abs(percent)) + QStringLiteral("%");
}

// The headphone mix: -1 (cue only) .. 1 (main only).
QString headphoneMixText(double value) {
    const int mainPercent = static_cast<int>(
            std::round((std::clamp(value, -1.0, 1.0) + 1.0) * 50.0));
    return QStringLiteral("Main %1% / Cue %2%").arg(mainPercent).arg(100 - mainPercent);
}

// About three significant digits (at most three decimals), trailing zeros
// dropped, a true minus sign; signed adds "+" above 0.
QString numberText(double number, bool signed_ = false) {
    const double magnitude = std::abs(number);
    const int decimals = magnitude > 0.0
            ? std::clamp(2 - static_cast<int>(std::floor(std::log10(magnitude))), 0, 3)
            : 0;
    QString digits = QString::number(magnitude, 'f', decimals);
    if (digits.contains(QChar('.'))) {
        while (digits.endsWith(QChar('0'))) {
            digits.chop(1);
        }
        if (digits.endsWith(QChar('.'))) {
            digits.chop(1);
        }
    }
    if (digits == QStringLiteral("0")) {
        return digits;
    }
    if (number < 0.0) {
        return kMinus + digits;
    }
    return signed_ ? QStringLiteral("+") + digits : digits;
}

// One decimal place, signed, for a parameter already in dB.
QString decibelValueText(double decibels) {
    const double tenths = std::round(decibels * 10.0);
    if (tenths == 0.0) {
        return QStringLiteral("0.0 dB");
    }
    return (tenths < 0.0 ? kMinus : QStringLiteral("+")) +
            QString::number(std::abs(tenths) / 10.0, 'f', 1) + QStringLiteral(" dB");
}

// An effect parameter's value with its manifest's units hint; empty for the
// "Unknown" hint, which reads as the knob position instead.
QString effectParameterText(EffectManifestParameter::UnitsHint unitsHint, double value) {
    using Unit = EffectManifestParameter::UnitsHint;
    switch (unitsHint) {
    case Unit::Unknown:
        return QString();
    case Unit::Decibel:
        return decibelValueText(value);
    case Unit::Hertz:
        if (std::abs(value) >= 1000.0) {
            return numberText(value / 1000.0) + QStringLiteral(" kHz");
        }
        return numberText(value) + QStringLiteral(" Hz");
    case Unit::Beats:
    case Unit::Beat:
        return numberText(value) + QStringLiteral(" beats");
    case Unit::Bar:
        return numberText(value) + QStringLiteral(" bars");
    case Unit::BPM:
        return numberText(value) + QStringLiteral(" BPM");
    case Unit::Cent:
        return numberText(value, true) + QStringLiteral(" cents");
    case Unit::Semitone12tet:
        return numberText(value, true) + QStringLiteral(" st");
    case Unit::Centimetre:
        return numberText(value) + QStringLiteral(" cm");
    case Unit::Degree:
        return numberText(value) + QChar(0x00B0);
    case Unit::Frame:
        return numberText(value) + QStringLiteral(" frames");
    case Unit::Inch:
        return numberText(value) + QStringLiteral(" in");
    case Unit::KiloHertz:
        return numberText(value) + QStringLiteral(" kHz");
    case Unit::Kilometer:
        return numberText(value) + QStringLiteral(" km");
    case Unit::Meter:
        return numberText(value) + QStringLiteral(" m");
    case Unit::MegaHertz:
        return numberText(value) + QStringLiteral(" MHz");
    case Unit::Mile:
        return numberText(value) + QStringLiteral(" mi");
    case Unit::Minute:
        return numberText(value) + QStringLiteral(" min");
    case Unit::Millmeter:
        return numberText(value) + QStringLiteral(" mm");
    case Unit::Millisecond:
        return numberText(value) + QStringLiteral(" ms");
    case Unit::Octave:
        return numberText(value) + QStringLiteral(" oct");
    case Unit::Percentage:
        return numberText(value) + QStringLiteral("%");
    case Unit::Seconds:
        return numberText(value) + QStringLiteral(" s");
    case Unit::Coefficient:
    case Unit::SampleRate:
    case Unit::Midinote:
    case Unit::Time:
        // No unit worth showing (built-in effects use SampleRate for Q).
        return numberText(value);
    }
    return QString();
}

QString knobPositionText(double parameter) {
    const int percent = static_cast<int>(std::round(std::clamp(parameter, 0.0, 1.0) * 100.0));
    return QString::number(percent) + QStringLiteral("%");
}

} // namespace

QString text(const KnobIdentity& knob,
        const KnobValue& value,
        const Options& options) {
    if (isAudioTaperGain(knob.key)) {
        return decibelText(value.value);
    }
    if (isDeckEq(knob.key)) {
        return options.mode == Mode::Precise
                ? decibelText(value.value)
                : neutralCentrePercentText(value.parameter);
    }
    if (isMainGroup(knob.key.group)) {
        if (knob.key.item == QStringLiteral("balance")) {
            return balanceText(value.value);
        }
        if (knob.key.item == QStringLiteral("headMix")) {
            return headphoneMixText(value.value);
        }
    }
    if (knob.unitsHint) {
        const QString parameterText = effectParameterText(*knob.unitsHint, value.value);
        if (!parameterText.isEmpty()) {
            return parameterText;
        }
    }
    return knobPositionText(value.parameter);
}

} // namespace knobreadout
} // namespace mixxx
