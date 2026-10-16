#pragma once

#include "preferences/usersettings.h"
#include "waveform/overviewlayout.h"
#include "waveform/overviewlinelayout.h"

// Loading and saving of the overview's bar and phrase line settings and hover
// readout settings under [Waveform]. Values that are missing or out of range
// in the config fall back to the defaults of the settings structs.
namespace mixxx {
namespace overviewlayout {

LineSettings loadLineSettings(const UserSettingsPointer& pConfig);
void saveLineSettings(const UserSettingsPointer& pConfig, const LineSettings& settings);

HoverReadoutSettings loadHoverReadoutSettings(const UserSettingsPointer& pConfig);
void saveHoverReadoutSettings(
        const UserSettingsPointer& pConfig, const HoverReadoutSettings& settings);

} // namespace overviewlayout
} // namespace mixxx
