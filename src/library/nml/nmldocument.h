#pragma once

#include <QIODevice>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// DJNote: GUI-free model of a Traktor .nml file (VERSION 19 and 20), read side.
// It keeps what the import, grid/key/cue, browse and re-read need;
// fields are optional/empty when the file does not carry them. Positions are
// milliseconds, as in the file.

namespace mixxx {
namespace nml {

/// Traktor cue types (CUE_V2@TYPE).
enum class NmlCueType {
    Cue = 0,
    FadeIn = 1,
    FadeOut = 2,
    Load = 3,
    Grid = 4,
    Loop = 5,
    Unknown = -1,
};

struct NmlCue {
    QString name;
    NmlCueType type = NmlCueType::Unknown;
    double startMillis = 0.0;
    double lengthMillis = 0.0;
    int hotcue = -1; // -1: no button
    int displOrder = 0;
    std::optional<int> color;      // CUE_V2@COLOR if the file has one
    std::optional<double> gridBpm; // CUE_V2/GRID@BPM (flexible grid marker)
};

struct NmlLocation {
    QString dir;    // raw, with Traktor's "/:" separators
    QString file;
    QString volume; // "C:", or a volume name on macOS
    QString volumeId;

    /// VOLUME + DIR + FILE joined as Traktor writes PLAYLIST PRIMARYKEY@KEY.
    QString primaryKey() const;
    /// Absolute path the file was recorded at (macOS: /Volumes/<VOLUME>...).
    QString recordedPath() const;
};

struct NmlEntry {
    QString title;
    QString artist;
    QString album;
    NmlLocation location;
    bool locked = false;           // ENTRY@LOCK
    QString keyText;               // INFO@KEY, e.g. "6m"
    std::optional<int> musicalKey; // MUSICAL_KEY@VALUE, 0-23
    std::optional<double> bpm;     // TEMPO@BPM
    std::optional<int> ranking;    // INFO@RANKING 0-255
    std::optional<int> color;      // INFO@COLOR
    double playtimeSeconds = 0.0;  // INFO@PLAYTIME_FLOAT, else PLAYTIME; 0 = unknown
    QList<NmlCue> cues;
};

struct NmlPlaylist {
    QString name;
    QString uuid;
    QStringList folderPath;  // enclosing FOLDER node names below $ROOT
    QStringList primaryKeys; // in playlist order, repeats kept
};

class NmlDocument {
  public:
    /// Parses a .nml. On failure returns false and sets *pError.
    bool read(QIODevice* pDevice, QString* pError = nullptr);
    bool readFile(const QString& path, QString* pError = nullptr);

    int version = 0;
    QString program;
    QList<NmlEntry> entries;
    QList<NmlPlaylist> playlists;

    /// Index into entries for a playlist primary key, or -1.
    int entryIndexForPrimaryKey(const QString& primaryKey) const;
};

/// "/:" -> "/"
QString fromTraktorSeparators(const QString& path);

} // namespace nml
} // namespace mixxx
