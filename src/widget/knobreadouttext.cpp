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

QString knobPositionText(double parameter) {
    const int percent = static_cast<int>(std::round(std::clamp(parameter, 0.0, 1.0) * 100.0));
    return QString::number(percent) + QStringLiteral("%");
}

} // namespace

QString text(const KnobIdentity& knob,
        const KnobValue& value,
        const Options& options) {
    Q_UNUSED(options);
    if (isAudioTaperGain(knob.key)) {
        return decibelText(value.value);
    }
    return knobPositionText(value.parameter);
}

} // namespace knobreadout
} // namespace mixxx
