#include "waveform/phosphor/phosphorsettings.h"

#include "preferences/configobject.h"

namespace mixxx::phosphor {

namespace {

const ConfigKey kDetailCfgKey(QStringLiteral("[Waveform]"), QStringLiteral("PhosphorDetail"));

} // namespace

WaveformDetail detailFromInt(int value) {
    if (value < static_cast<int>(WaveformDetail::Full) ||
            value > static_cast<int>(kLowestDetail)) {
        return kDefaultDetail;
    }
    return static_cast<WaveformDetail>(value);
}

SINT dataBytes(WaveformDetail detail, SINT frames) {
    const SINT n = blockSizeOf(detail);
    if (n == 1) {
        return frames * 2;
    }
    // Two int16 values per block.
    return (frames + n - 1) / n * 4;
}

WaveformDetail effectiveDetail(WaveformDetail chosen, SINT frames) {
    int tier = static_cast<int>(chosen);
    while (tier < static_cast<int>(kLowestDetail) &&
            dataBytes(static_cast<WaveformDetail>(tier), frames) > kMaxDataBytes) {
        ++tier;
    }
    return static_cast<WaveformDetail>(tier);
}

PhosphorSettings loadSettings(const UserSettingsPointer& pConfig) {
    PhosphorSettings settings;
    settings.detail = detailFromInt(
            pConfig->getValue(kDetailCfgKey, static_cast<int>(settings.detail)));
    return settings;
}

void saveSettings(const UserSettingsPointer& pConfig, const PhosphorSettings& settings) {
    pConfig->setValue(kDetailCfgKey, static_cast<int>(settings.detail));
}

} // namespace mixxx::phosphor
