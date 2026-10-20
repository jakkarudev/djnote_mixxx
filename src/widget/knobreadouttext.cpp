#include "widget/knobreadouttext.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace mixxx {
namespace knobreadout {

namespace {

const QString kMinus = QString(QChar(0x2212));

// User-visible words go through Qt's translation; untranslated (as in the
// tests) the English source text is returned. Mark source strings with
// QT_TRANSLATE_NOOP("KnobReadout", ...) so lupdate finds them.
QString translated(const char* sourceText) {
    return QCoreApplication::translate("KnobReadout", sourceText);
}

// A number with its unit, e.g. "%1 Hz".
QString withUnit(const QString& number, const char* unitPattern) {
    return translated(unitPattern).arg(number);
}

// The sign of a rounded value: a true minus sign below 0, "+" above 0 and
// nothing on 0 (a value that rounds to 0 has no sign).
QString signOf(double rounded) {
    if (rounded < 0.0) {
        return kMinus;
    }
    return rounded > 0.0 ? QStringLiteral("+") : QString();
}

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

// Decibels with one decimal place, signed (see signOf()).
QString signedDecibelText(double decibels) {
    const double tenths = std::round(decibels * 10.0);
    return withUnit(signOf(tenths) + QString::number(std::abs(tenths) / 10.0, 'f', 1),
            QT_TRANSLATE_NOOP("KnobReadout", "%1 dB"));
}

// A linear gain factor in dB; "minus infinity dB" at 0.
QString gainDecibelText(double factor) {
    if (!(factor > 0.0)) {
        return withUnit(kMinus + QChar(0x221E), QT_TRANSLATE_NOOP("KnobReadout", "%1 dB"));
    }
    return signedDecibelText(20.0 * std::log10(factor));
}

// The knob parameters of a deck's EQ-rack effect, e.g.
// [EqualizerRack1_[Channel1]_Effect1],parameter1. Their manifests say
// "Unknown" units, so the group tells them apart.
bool isDeckEq(const ConfigKey& key) {
    return key.group.startsWith(QStringLiteral("[EqualizerRack")) &&
            key.item.startsWith(QStringLiteral("parameter"));
}

// A whole percent, signed (see signOf()).
QString signedPercentText(double percent) {
    const double rounded = std::round(percent);
    return signOf(rounded) + QString::number(static_cast<int>(std::abs(rounded))) +
            QStringLiteral("%");
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
        return translated(QT_TRANSLATE_NOOP("KnobReadout", "C"));
    }
    return (percent < 0 ? translated(QT_TRANSLATE_NOOP("KnobReadout", "L %1%"))
                        : translated(QT_TRANSLATE_NOOP("KnobReadout", "R %1%")))
            .arg(std::abs(percent));
}

// The headphone mix: -1 (cue only) .. 1 (main only).
QString headphoneMixText(double value) {
    const int mainPercent = static_cast<int>(
            std::round((std::clamp(value, -1.0, 1.0) + 1.0) * 50.0));
    return translated(QT_TRANSLATE_NOOP("KnobReadout", "Main %1% / Cue %2%"))
            .arg(mainPercent)
            .arg(100 - mainPercent);
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
    // Unsigned numbers still keep their minus sign.
    return (signed_ || number < 0.0 ? signOf(number) : QString()) + digits;
}

// An effect parameter's value with its manifest's units hint; empty for the
// "Unknown" hint, which reads as the knob position instead.
QString effectParameterText(EffectManifestParameter::UnitsHint unitsHint, double value) {
    using Unit = EffectManifestParameter::UnitsHint;
    switch (unitsHint) {
    case Unit::Unknown:
        return QString();
    case Unit::Decibel:
        return signedDecibelText(value);
    case Unit::Hertz:
        if (std::abs(value) >= 1000.0) {
            return withUnit(numberText(value / 1000.0), QT_TRANSLATE_NOOP("KnobReadout", "%1 kHz"));
        }
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 Hz"));
    case Unit::Beats:
    case Unit::Beat:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 beats"));
    case Unit::Bar:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 bars"));
    case Unit::BPM:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 BPM"));
    case Unit::Cent:
        return withUnit(numberText(value, true), QT_TRANSLATE_NOOP("KnobReadout", "%1 cents"));
    case Unit::Semitone12tet:
        return withUnit(numberText(value, true), QT_TRANSLATE_NOOP("KnobReadout", "%1 st"));
    case Unit::Centimetre:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 cm"));
    case Unit::Degree:
        return numberText(value) + QChar(0x00B0);
    case Unit::Frame:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 frames"));
    case Unit::Inch:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 in"));
    case Unit::KiloHertz:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 kHz"));
    case Unit::Kilometer:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 km"));
    case Unit::Meter:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 m"));
    case Unit::MegaHertz:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 MHz"));
    case Unit::Mile:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 mi"));
    case Unit::Minute:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 min"));
    case Unit::Millmeter:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 mm"));
    case Unit::Millisecond:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 ms"));
    case Unit::Octave:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 oct"));
    case Unit::Percentage:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1%"));
    case Unit::Seconds:
        return withUnit(numberText(value), QT_TRANSLATE_NOOP("KnobReadout", "%1 s"));
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

// The full (not compact) text, by the unit table in knobreadouttext.h.
QString readoutText(const KnobIdentity& knob, const KnobValue& value, Mode mode) {
    if (isAudioTaperGain(knob.key)) {
        return gainDecibelText(value.value);
    }
    if (isDeckEq(knob.key)) {
        return mode == Mode::Precise
                ? gainDecibelText(value.value)
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

} // namespace

QString text(const KnobIdentity& knob,
        const KnobValue& value,
        const Options& options) {
    const QString full = readoutText(knob, value, options.mode);
    return options.compact ? compactForm(full) : full;
}

QString compactForm(const QString& fullText) {
    // [label ][sign]number-or-infinity[ unit]
    static const QRegularExpression kPattern(QStringLiteral(
            "^(?:(\\p{L}+)\\s+)?([+\\x{2212}-]?)(\\d+(?:\\.\\d+)?|\\x{221E})\\s*\\D*$"));
    const QRegularExpressionMatch match = kPattern.match(fullText);
    if (!match.hasMatch()) {
        return fullText;
    }
    const QString sign = match.captured(2) == QStringLiteral("+") ? QString() : match.captured(2);
    return match.captured(1) + sign + match.captured(3);
}

} // namespace knobreadout
} // namespace mixxx
