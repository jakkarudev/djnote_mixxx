#pragma once

#include <QString>

// DJNote: the record label of an audio file for INFO@LABEL.
//
// Mixxx only models the label behind __EXTRA_METADATA__, which no build
// defines, so Track::getMetadata().getAlbumInfo() has no recordLabel here and
// the library never reads it. The export reads the file's tag itself.

namespace mixxx {
namespace nml {

/// ID3v2 TPUB, MP4 label atom, Vorbis/APE LABEL, ORGANIZATION or PUBLISHER of
/// the file at `filePath`; empty when there is none or the file is unreadable.
QString nmlRecordLabelFromFile(const QString& filePath);

} // namespace nml
} // namespace mixxx
