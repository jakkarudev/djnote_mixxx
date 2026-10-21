#include "library/traktor/traktorkey.h"

namespace mixxx::traktor {

mixxx::track::io::key::ChromaticKey keyFromTraktorMusicalKeyValue(
        const QString& value) {
    // Traktor's values 0..23 map onto C_MAJOR..B_MINOR
    constexpr int kMaxTraktorValue =
            mixxx::track::io::key::B_MINOR - mixxx::track::io::key::C_MAJOR;
    bool ok = false;
    const int traktorValue = value.toInt(&ok);
    if (!ok || traktorValue < 0 || traktorValue > kMaxTraktorValue) {
        return mixxx::track::io::key::INVALID;
    }
    return static_cast<mixxx::track::io::key::ChromaticKey>(
            mixxx::track::io::key::C_MAJOR + traktorValue);
}

} // namespace mixxx::traktor
