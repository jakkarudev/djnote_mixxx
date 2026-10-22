#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

#include "library/nml/nmldocument.h"

// DJNote: writer of Traktor .nml files, VERSION 20, in the exact byte layout
// of Traktor Pro 4.0.2 (ADR 0010, docs/research/traktor-nml.md addendum):
// declaration line, no BOM, LF, no indentation, no self-closed elements,
// ENTRY and LOCATION on one line, 6-decimal floats, yyyy/M/d dates.
//
// GUI-free and Track-free: the caller fills NmlWriteDocument. The export
// preparation fills the optional fields of NmlWriteEntry (tempo,
// key, ranking, colour, lock, cues); the writer already emits them in
// Traktor's element order when they are set.

namespace mixxx {
namespace nml {

struct NmlWriteEntry {
    // Absolute path the LOCATION points at (the copy, or the original).
    QString path;

    QString title;
    QString artist;
    QString album;
    QString trackNumber; // ALBUM@TRACK, empty: omitted
    QString genre;
    QString comment;
    QString label;
    QString releaseDate; // free text, normalised to yyyy/M/d when it is a date or a year
    double playtimeSeconds = 0.0; // 0: PLAYTIME omitted
    int bitrateBitsPerSecond = 0; // 0: BITRATE omitted

    // Preparation. All optional.
    bool locked = false;
    std::optional<double> bpm;      // TEMPO@BPM
    QString keyText;                // INFO@KEY
    std::optional<int> musicalKey;  // MUSICAL_KEY@VALUE 0-23
    std::optional<int> ranking;     // INFO@RANKING 0-255
    std::optional<int> color;       // INFO@COLOR 1-7
    QList<NmlCue> cues;             // written in the given order
    int playCount = 0;              // INFO@PLAYCOUNT, 0: omitted
    QDate lastPlayed;               // INFO@LAST_PLAYED, invalid: omitted
};

struct NmlWriteDocument {
    QString playlistName;
    QString playlistUuid; // 32 lowercase hex digits; empty: a fresh one
    // Collection entries, one per distinct file.
    QList<NmlWriteEntry> entries;
    // Playlist order as indexes into `entries`, repeats allowed.
    QList<int> playlistOrder;
    // Time of the export (UTC): MODIFIED_*, IMPORT_DATE, LOCK_MODIFICATION_TIME.
    QDateTime exportTimeUtc;
};

/// Serialises `doc` as a Traktor Pro 4.0.2 VERSION 20 playlist export.
QByteArray writeNml(const NmlWriteDocument& doc);

/// Splits an absolute path into Traktor's VOLUME/DIR/FILE (with "/:" in DIR).
/// Windows "C:/a/b.mp3" -> VOLUME "C:", macOS "/Volumes/X/a/b.mp3" -> VOLUME
/// "X", otherwise VOLUME is empty. volumeId is set equal to volume.
NmlLocation locationFromPath(const QString& absolutePath);

/// "6.000000" style number as Traktor writes floats.
QString nmlFloat(double value);

/// yyyy/M/d from "2021-04-16", "2021/4/16" or "2021" (-> 2021/1/1); empty
/// if `text` is neither.
QString nmlReleaseDate(const QString& text);

/// Fresh 32 hex digit lowercase UUID, as in PLAYLIST@UUID.
QString newNmlUuid();

} // namespace nml
} // namespace mixxx
