#pragma once

#include "preferences/usersettings.h"
#include "waveform/scrollinglinelayout.h"

// Loading and saving of the scrolling waveform's beat, bar and step line
// settings under [Waveform], in DJNote's own keys (upstream's beatGridAlpha is
// left alone). Values that are missing or out of range in the config fall back
// to the defaults of the settings struct (opacities and widths are clamped; an
// invalid colour means the skin colour).
namespace mixxx {
namespace scrollinglinelayout {

LineSettings loadLineSettings(const UserSettingsPointer& pConfig);
void saveLineSettings(const UserSettingsPointer& pConfig, const LineSettings& settings);

} // namespace scrollinglinelayout
} // namespace mixxx
