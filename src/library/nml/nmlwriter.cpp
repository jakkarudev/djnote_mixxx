#include "library/nml/nmlwriter.h"

#include <QDate>
#include <cmath>
#include <QRegularExpression>
#include <QUuid>

namespace mixxx {
namespace nml {

namespace {

QString escapeAttribute(const QString& text) {
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&':
            out += QStringLiteral("&amp;");
            break;
        case '<':
            out += QStringLiteral("&lt;");
            break;
        case '>':
            out += QStringLiteral("&gt;");
            break;
        case '"':
            out += QStringLiteral("&quot;");
            break;
        case '\n':
            out += QStringLiteral("&#10;");
            break;
        case '\r':
            out += QStringLiteral("&#13;");
            break;
        case '\t':
            out += QStringLiteral("&#9;");
            break;
        default:
            out += c;
        }
    }
    return out;
}

// Appends ` NAME="value"`; skipped when value is empty.
void attr(QString* pOut, const char* name, const QString& value) {
    if (value.isEmpty()) {
        return;
    }
    *pOut += QLatin1Char(' ') + QLatin1String(name) + QStringLiteral("=\"") +
            escapeAttribute(value) + QLatin1Char('"');
}

QString dateText(const QDate& date) {
    return QStringLiteral("%1/%2/%3").arg(date.year()).arg(date.month()).arg(date.day());
}

int secondsSinceMidnight(const QTime& time) {
    return QTime(0, 0).secsTo(time);
}

} // namespace

QString nmlFloat(double value) {
    return QString::number(value, 'f', 6);
}

QString nmlReleaseDate(const QString& text) {
    static const QRegularExpression kDate(
            QStringLiteral("^\\s*(\\d{4})(?:[-/](\\d{1,2})(?:[-/](\\d{1,2}))?)?(?:[T ].*)?\\s*$"));
    const QRegularExpressionMatch match = kDate.match(text);
    if (!match.hasMatch()) {
        return QString();
    }
    const int year = match.captured(1).toInt();
    const int month = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
    const int day = match.captured(3).isEmpty() ? 1 : match.captured(3).toInt();
    if (!QDate::isValid(year, month, day)) {
        return QString();
    }
    return QStringLiteral("%1/%2/%3").arg(year).arg(month).arg(day);
}

QString newNmlUuid() {
    return QUuid::createUuid().toString(QUuid::Id128);
}

NmlLocation locationFromPath(const QString& absolutePath) {
    NmlLocation location;
    QString path = absolutePath;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    const QString kVolumes = QStringLiteral("/Volumes/");
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        location.volume = path.left(2);
        path = path.mid(2);
    } else if (path.startsWith(kVolumes)) {
        const int end = path.indexOf(QLatin1Char('/'), kVolumes.size());
        if (end > 0) {
            location.volume = path.mid(kVolumes.size(), end - kVolumes.size());
            path = path.mid(end);
        }
    }
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    QString dir = slash >= 0 ? path.left(slash + 1) : QStringLiteral("/");
    location.file = path.mid(slash + 1);
    location.dir = dir.replace(QLatin1Char('/'), QStringLiteral("/:"));
    location.volumeId = location.volume;
    return location;
}

QByteArray writeNml(const NmlWriteDocument& doc) {
    const QDateTime now = doc.exportTimeUtc.isValid()
            ? doc.exportTimeUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    const QString today = dateText(now.date());
    const QString modifiedTime = QString::number(secondsSinceMidnight(now.time()));

    QString out;
    out += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
    out += QStringLiteral(
            "<NML VERSION=\"20\"><HEAD COMPANY=\"www.native-instruments.com\" "
            "PROGRAM=\"Traktor Pro 4\"></HEAD>\n");
    out += QStringLiteral("<COLLECTION ENTRIES=\"%1\">").arg(doc.entries.size());

    QStringList keys;
    keys.reserve(doc.entries.size());
    for (const NmlWriteEntry& e : doc.entries) {
        const NmlLocation location = locationFromPath(e.path);
        keys.append(location.primaryKey());

        out += QStringLiteral("<ENTRY MODIFIED_DATE=\"") + today +
                QStringLiteral("\" MODIFIED_TIME=\"") + modifiedTime + QLatin1Char('"');
        if (e.locked) {
            out += QStringLiteral(" LOCK=\"1\" LOCK_MODIFICATION_TIME=\"") +
                    now.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")) + QLatin1Char('"');
        }
        attr(&out, "TITLE", e.title);
        attr(&out, "ARTIST", e.artist);
        out += QLatin1Char('>');

        out += QStringLiteral("<LOCATION");
        attr(&out, "DIR", location.dir);
        attr(&out, "FILE", location.file);
        attr(&out, "VOLUME", location.volume);
        attr(&out, "VOLUMEID", location.volumeId);
        out += QStringLiteral("></LOCATION>\n");

        if (!e.album.isEmpty() || !e.trackNumber.isEmpty()) {
            out += QStringLiteral("<ALBUM");
            attr(&out, "OF_TRACKS", e.trackTotal);
            attr(&out, "TRACK", e.trackNumber);
            attr(&out, "TITLE", e.album);
            out += QStringLiteral("></ALBUM>\n");
        }
        out += QStringLiteral("<MODIFICATION_INFO AUTHOR_TYPE=\"user\"></MODIFICATION_INFO>\n");

        out += QStringLiteral("<INFO");
        if (e.bitrateBitsPerSecond > 0) {
            attr(&out, "BITRATE", QString::number(e.bitrateBitsPerSecond));
        }
        attr(&out, "GENRE", e.genre);
        attr(&out, "LABEL", e.label);
        attr(&out, "COMMENT", e.comment);
        // Order verified against Traktor 4.0.2's collection.nml and test.nml:
        // KEY, PLAYCOUNT, PLAYTIME, PLAYTIME_FLOAT, RANKING, IMPORT_DATE,
        // LAST_PLAYED. COLOR has no sample anywhere; it follows RANKING.
        attr(&out, "KEY", e.keyText);
        if (e.playCount > 0) {
            attr(&out, "PLAYCOUNT", QString::number(e.playCount));
        }
        if (e.playtimeSeconds > 0.0) {
            attr(&out, "PLAYTIME", QString::number(static_cast<int>(std::ceil(e.playtimeSeconds))));
            attr(&out, "PLAYTIME_FLOAT", nmlFloat(e.playtimeSeconds));
        }
        if (e.ranking) {
            attr(&out, "RANKING", QString::number(*e.ranking));
        }
        if (e.color) {
            attr(&out, "COLOR", QString::number(*e.color));
        }
        attr(&out, "IMPORT_DATE", today);
        if (e.lastPlayed.isValid()) {
            attr(&out, "LAST_PLAYED", dateText(e.lastPlayed));
        }
        attr(&out, "RELEASE_DATE", nmlReleaseDate(e.releaseDate));
        // Traktor sets bit 0x10 together with LOCK (25 of 26 locked entries of
        // its collection have it, no unlocked one).
        attr(&out, "FLAGS", QString::fromLatin1(e.locked ? "28" : "12"));
        out += QStringLiteral("></INFO>\n");

        if (e.bpm) {
            out += QStringLiteral("<TEMPO BPM=\"") + nmlFloat(*e.bpm) +
                    QStringLiteral("\" BPM_QUALITY=\"100.000000\"></TEMPO>\n");
        }
        if (e.musicalKey) {
            out += QStringLiteral("<MUSICAL_KEY VALUE=\"") + QString::number(*e.musicalKey) +
                    QStringLiteral("\"></MUSICAL_KEY>\n");
        }
        for (const NmlCue& cue : e.cues) {
            out += QStringLiteral("<CUE_V2");
            out += QStringLiteral(" NAME=\"") + escapeAttribute(cue.name) + QLatin1Char('"');
            out += QStringLiteral(" DISPL_ORDER=\"%1\" TYPE=\"%2\"")
                           .arg(cue.displOrder)
                           .arg(static_cast<int>(cue.type));
            out += QStringLiteral(" START=\"") + nmlFloat(cue.startMillis) +
                    QStringLiteral("\" LEN=\"") + nmlFloat(cue.lengthMillis) +
                    QStringLiteral("\" REPEATS=\"-1\" HOTCUE=\"%1\"").arg(cue.hotcue);
            if (cue.color) {
                out += QStringLiteral(" COLOR=\"#%1\"")
                               .arg(static_cast<uint>(*cue.color) & 0xFFFFFFu, 6, 16, QLatin1Char('0'))
                               .toUpper();
            }
            out += QLatin1Char('>');
            if (cue.gridBpm) {
                out += QStringLiteral("<GRID BPM=\"") + nmlFloat(*cue.gridBpm) +
                        QStringLiteral("\"></GRID>\n");
            }
            out += QStringLiteral("</CUE_V2>\n");
        }
        out += QStringLiteral("</ENTRY>\n");
    }
    out += QStringLiteral("</COLLECTION>\n");
    out += QStringLiteral("<SETS ENTRIES=\"0\"></SETS>\n");

    out += QStringLiteral(
            "<PLAYLISTS><NODE TYPE=\"FOLDER\" NAME=\"$ROOT\"><SUBNODES COUNT=\"1\">"
            "<NODE TYPE=\"PLAYLIST\" NAME=\"") +
            escapeAttribute(doc.playlistName) +
            QStringLiteral("\"><PLAYLIST ENTRIES=\"%1\" TYPE=\"LIST\" UUID=\"%2\">")
                    .arg(doc.playlistOrder.size())
                    .arg(doc.playlistUuid.isEmpty() ? newNmlUuid() : doc.playlistUuid);
    for (const int index : doc.playlistOrder) {
        if (index < 0 || index >= keys.size()) {
            continue;
        }
        out += QStringLiteral("<ENTRY><PRIMARYKEY TYPE=\"TRACK\" KEY=\"") +
                escapeAttribute(keys.at(index)) +
                QStringLiteral("\"></PRIMARYKEY>\n</ENTRY>\n");
    }
    out += QStringLiteral(
            "</PLAYLIST>\n</NODE>\n</SUBNODES>\n</NODE>\n</PLAYLISTS>\n"
            "<INDEXING></INDEXING>\n</NML>\n");
    return out.toUtf8();
}

} // namespace nml
} // namespace mixxx
