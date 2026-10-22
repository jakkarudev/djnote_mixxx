#include "library/nml/nmldocument.h"

#include <QFile>
#include <QXmlStreamReader>

namespace mixxx {
namespace nml {

namespace {

std::optional<int> optInt(const QXmlStreamAttributes& a, const char* name) {
    if (!a.hasAttribute(QLatin1String(name))) {
        return std::nullopt;
    }
    bool ok = false;
    const int v = a.value(QLatin1String(name)).toInt(&ok);
    return ok ? std::optional<int>(v) : std::nullopt;
}

std::optional<double> optDouble(const QXmlStreamAttributes& a, const char* name) {
    if (!a.hasAttribute(QLatin1String(name))) {
        return std::nullopt;
    }
    bool ok = false;
    const double v = a.value(QLatin1String(name)).toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

QString str(const QXmlStreamAttributes& a, const char* name) {
    return a.value(QLatin1String(name)).toString();
}

NmlCueType cueTypeFromInt(int t) {
    switch (t) {
    case 0:
        return NmlCueType::Cue;
    case 1:
        return NmlCueType::FadeIn;
    case 2:
        return NmlCueType::FadeOut;
    case 3:
        return NmlCueType::Load;
    case 4:
        return NmlCueType::Grid;
    case 5:
        return NmlCueType::Loop;
    default:
        return NmlCueType::Unknown;
    }
}

struct NodeFrame {
    QString type;
    QString name;
};

} // namespace

QString fromTraktorSeparators(const QString& path) {
    QString result = path;
    return result.replace(QStringLiteral("/:"), QStringLiteral("/"));
}

QString NmlLocation::primaryKey() const {
    return volume + dir + file;
}

QString NmlLocation::recordedPath() const {
    const QString dirPath = fromTraktorSeparators(dir);
#if defined(__APPLE__)
    return QStringLiteral("/Volumes/") + volume + dirPath + file;
#else
    return volume + dirPath + file;
#endif
}

int NmlDocument::entryIndexForPrimaryKey(const QString& primaryKey) const {
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).location.primaryKey() == primaryKey) {
            return i;
        }
    }
    return -1;
}

bool NmlDocument::readFile(const QString& path, QString* pError) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (pError) {
            *pError = file.errorString();
        }
        return false;
    }
    return read(&file, pError);
}

bool NmlDocument::read(QIODevice* pDevice, QString* pError) {
    version = 0;
    program.clear();
    entries.clear();
    playlists.clear();

    QXmlStreamReader xml(pDevice);
    QStringList stack; // element names, current element last
    QList<NodeFrame> nodes;
    bool sawNml = false;
    NmlEntry entry;
    NmlCue cue;
    NmlPlaylist playlist;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = xml.name().toString();
            const auto a = xml.attributes();
            const QString parent = stack.isEmpty() ? QString() : stack.last();
            stack.append(name);
            if (name == QLatin1String("NML") && stack.size() == 1) {
                sawNml = true;
                version = a.value(QLatin1String("VERSION")).toInt();
            } else if (name == QLatin1String("HEAD")) {
                program = str(a, "PROGRAM");
            } else if (name == QLatin1String("ENTRY") && parent == QLatin1String("COLLECTION")) {
                entry = NmlEntry();
                entry.title = str(a, "TITLE");
                entry.artist = str(a, "ARTIST");
                entry.locked = a.value(QLatin1String("LOCK")) == QLatin1String("1");
            } else if (name == QLatin1String("LOCATION") && parent == QLatin1String("ENTRY")) {
                entry.location.dir = str(a, "DIR");
                entry.location.file = str(a, "FILE");
                entry.location.volume = str(a, "VOLUME");
                entry.location.volumeId = str(a, "VOLUMEID");
            } else if (name == QLatin1String("ALBUM") && parent == QLatin1String("ENTRY")) {
                entry.album = str(a, "TITLE");
            } else if (name == QLatin1String("INFO") && parent == QLatin1String("ENTRY")) {
                entry.keyText = str(a, "KEY");
                entry.ranking = optInt(a, "RANKING");
                entry.color = optInt(a, "COLOR");
            } else if (name == QLatin1String("TEMPO") && parent == QLatin1String("ENTRY")) {
                entry.bpm = optDouble(a, "BPM");
            } else if (name == QLatin1String("MUSICAL_KEY") && parent == QLatin1String("ENTRY")) {
                entry.musicalKey = optInt(a, "VALUE");
            } else if (name == QLatin1String("CUE_V2") && parent == QLatin1String("ENTRY")) {
                cue = NmlCue();
                cue.name = str(a, "NAME");
                cue.type = cueTypeFromInt(a.value(QLatin1String("TYPE")).toInt());
                cue.startMillis = a.value(QLatin1String("START")).toDouble();
                cue.lengthMillis = a.value(QLatin1String("LEN")).toDouble();
                cue.hotcue = optInt(a, "HOTCUE").value_or(-1);
                cue.displOrder = optInt(a, "DISPL_ORDER").value_or(0);
                cue.color = optInt(a, "COLOR");
            } else if (name == QLatin1String("GRID") && parent == QLatin1String("CUE_V2")) {
                cue.gridBpm = optDouble(a, "BPM");
            } else if (name == QLatin1String("NODE")) {
                nodes.append({str(a, "TYPE"), str(a, "NAME")});
            } else if (name == QLatin1String("PLAYLIST") && parent == QLatin1String("NODE") &&
                    !nodes.isEmpty()) {
                playlist = NmlPlaylist();
                playlist.name = nodes.last().name;
                playlist.uuid = str(a, "UUID");
                // Enclosing folders below the first ($ROOT) one.
                for (int i = 1; i < nodes.size() - 1; ++i) {
                    if (nodes.at(i).type == QLatin1String("FOLDER")) {
                        playlist.folderPath.append(nodes.at(i).name);
                    }
                }
            } else if (name == QLatin1String("PRIMARYKEY") && stack.size() >= 3 &&
                    stack.at(stack.size() - 3) == QLatin1String("PLAYLIST")) {
                playlist.primaryKeys.append(str(a, "KEY"));
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("ENTRY") && stack.size() == 3 &&
                    stack.at(1) == QLatin1String("COLLECTION")) {
                entries.append(entry);
            } else if (name == QLatin1String("CUE_V2") && stack.size() >= 2 &&
                    stack.at(stack.size() - 2) == QLatin1String("ENTRY")) {
                entry.cues.append(cue);
            } else if (name == QLatin1String("PLAYLIST") && stack.size() >= 2 &&
                    stack.at(stack.size() - 2) == QLatin1String("NODE")) {
                playlists.append(playlist);
            } else if (name == QLatin1String("NODE") && !nodes.isEmpty()) {
                nodes.removeLast();
            }
            if (!stack.isEmpty()) {
                stack.removeLast();
            }
        }
    }
    if (xml.hasError()) {
        if (pError) {
            *pError = xml.errorString();
        }
        return false;
    }
    if (!sawNml) {
        if (pError) {
            *pError = QStringLiteral("Not a Traktor NML file");
        }
        return false;
    }
    return true;
}

} // namespace nml
} // namespace mixxx
