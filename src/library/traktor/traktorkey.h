#pragma once

#include <QString>

#include "proto/keys.pb.h"

namespace mixxx::traktor {

/// Converts the VALUE attribute of a Traktor collection.nml MUSICAL_KEY element
/// to a Mixxx key.
///
/// Traktor numbers its keys by pitch class, with C = 0 ... B = 11 for major
/// keys and 12 ... 23 for the minor keys (C minor = 12, B minor = 23). Mixxx's
/// ChromaticKey starts at INVALID = 0, C_MAJOR = 1 ... B_MINOR = 24, so the
/// two numberings differ by one.
///
/// Returns INVALID if the value is empty, not an integer or outside 0-23.
mixxx::track::io::key::ChromaticKey keyFromTraktorMusicalKeyValue(
        const QString& value);

} // namespace mixxx::traktor
