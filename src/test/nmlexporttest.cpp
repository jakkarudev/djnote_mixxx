#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmldocument.h"
#include "library/nml/nmlexport.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlwriter.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"

// DJNote: NML export. Seams: writeNml() (bytes), NmlExportWorker (a
// temporary folder, like trackexport_test), nmlExportItemsForPlaylist().

using namespace mixxx::nml;

namespace {

QDateTime exportTime() {
    return QDateTime(QDate(2026, 10, 22), QTime(12, 34, 56), Qt::UTC);
}

QByteArray readAll(const QString& path) {
    QFile f(path);
    EXPECT_TRUE(f.open(QIODevice::ReadOnly));
    return f.readAll();
}

void writeFile(const QString& path, const QByteArray& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(content);
}

NmlWriteEntry entryFor(const QString& path, const QString& title = QString()) {
    NmlWriteEntry e;
    e.path = path;
    e.title = title.isEmpty() ? QFileInfo(path).completeBaseName() : title;
    return e;
}

NmlExportJob jobFor(const QString& nmlPath, const QList<NmlWriteEntry>& items, bool copy = true) {
    NmlExportJob job;
    job.nmlPath = nmlPath;
    job.playlistName = QStringLiteral("Preparation");
    job.items = items;
    job.copyTracks = copy;
    job.exportTimeUtc = exportTime();
    return job;
}

NmlExportResult runExport(const NmlExportJob& job) {
    NmlExportWorker worker(job);
    worker.run();
    return worker.result();
}

} // namespace

TEST(NmlWriterTest, MatchesTraktor402Layout) {
    NmlWriteEntry e;
    e.path = QStringLiteral("C:/Music/Export/Salona - Connected.mp3");
    e.title = QStringLiteral("Connected");
    e.artist = QStringLiteral("Salona & Co");
    e.album = QStringLiteral("Traktor Pro 4");
    e.trackNumber = QStringLiteral("3");
    e.genre = QStringLiteral("Trance");
    e.comment = QStringLiteral("say \"hi\"");
    e.releaseDate = QStringLiteral("2021-04-16");
    e.playtimeSeconds = 137.11673;
    e.bitrateBitsPerSecond = 320000;

    NmlWriteDocument doc;
    doc.playlistName = QStringLiteral("Preparation");
    doc.playlistUuid = QStringLiteral("338613851b2e4bddb2926ac6a11ed148");
    doc.entries = {e};
    doc.playlistOrder = {0, 0};
    doc.exportTimeUtc = exportTime();

    const QByteArray expected =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
            "<NML VERSION=\"20\"><HEAD COMPANY=\"www.native-instruments.com\" PROGRAM=\"Traktor Pro 4\"></HEAD>\n"
            "<COLLECTION ENTRIES=\"1\"><ENTRY MODIFIED_DATE=\"2026/10/22\" MODIFIED_TIME=\"45296\" "
            "TITLE=\"Connected\" ARTIST=\"Salona &amp; Co\">"
            "<LOCATION DIR=\"/:Music/:Export/:\" FILE=\"Salona - Connected.mp3\" VOLUME=\"C:\" VOLUMEID=\"C:\"></LOCATION>\n"
            "<ALBUM TRACK=\"3\" TITLE=\"Traktor Pro 4\"></ALBUM>\n"
            "<MODIFICATION_INFO AUTHOR_TYPE=\"user\"></MODIFICATION_INFO>\n"
            "<INFO BITRATE=\"320000\" GENRE=\"Trance\" COMMENT=\"say &quot;hi&quot;\" "
            "PLAYTIME=\"138\" PLAYTIME_FLOAT=\"137.116730\" IMPORT_DATE=\"2026/10/22\" "
            "RELEASE_DATE=\"2021/4/16\" FLAGS=\"12\"></INFO>\n"
            "</ENTRY>\n"
            "</COLLECTION>\n"
            "<SETS ENTRIES=\"0\"></SETS>\n"
            "<PLAYLISTS><NODE TYPE=\"FOLDER\" NAME=\"$ROOT\"><SUBNODES COUNT=\"1\">"
            "<NODE TYPE=\"PLAYLIST\" NAME=\"Preparation\">"
            "<PLAYLIST ENTRIES=\"2\" TYPE=\"LIST\" UUID=\"338613851b2e4bddb2926ac6a11ed148\">"
            "<ENTRY><PRIMARYKEY TYPE=\"TRACK\" KEY=\"C:/:Music/:Export/:Salona - Connected.mp3\"></PRIMARYKEY>\n"
            "</ENTRY>\n"
            "<ENTRY><PRIMARYKEY TYPE=\"TRACK\" KEY=\"C:/:Music/:Export/:Salona - Connected.mp3\"></PRIMARYKEY>\n"
            "</ENTRY>\n"
            "</PLAYLIST>\n"
            "</NODE>\n"
            "</SUBNODES>\n"
            "</NODE>\n"
            "</PLAYLISTS>\n"
            "<INDEXING></INDEXING>\n"
            "</NML>\n";
    EXPECT_EQ(expected, writeNml(doc));
}

TEST(NmlWriterTest, PreparationFieldsAreWrittenInTraktorOrder) {
    NmlWriteEntry e = entryFor(QStringLiteral("C:/a/x.mp3"), QStringLiteral("X"));
    e.locked = true;
    e.bpm = 137.004074;
    e.musicalKey = 20;
    NmlCue grid;
    grid.name = QStringLiteral("Beat Marker");
    grid.type = NmlCueType::Grid;
    grid.startMillis = 53.75978;
    grid.gridBpm = 137.004074;
    NmlCue hot;
    hot.name = QStringLiteral("n.n.");
    hot.type = NmlCueType::Cue;
    hot.startMillis = 53.75978;
    hot.hotcue = 0;
    e.cues = {grid, hot};

    NmlWriteDocument doc;
    doc.entries = {e};
    doc.playlistOrder = {0};
    doc.exportTimeUtc = exportTime();
    const QString text = QString::fromUtf8(writeNml(doc));
    EXPECT_TRUE(text.contains(QStringLiteral(
            "MODIFIED_TIME=\"45296\" LOCK=\"1\" LOCK_MODIFICATION_TIME=\"2026-10-22T12:34:56\" TITLE=\"X\">")));
    EXPECT_TRUE(text.contains(QStringLiteral(
            "<TEMPO BPM=\"137.004074\" BPM_QUALITY=\"100.000000\"></TEMPO>\n"
            "<MUSICAL_KEY VALUE=\"20\"></MUSICAL_KEY>\n"
            "<CUE_V2 NAME=\"Beat Marker\" DISPL_ORDER=\"0\" TYPE=\"4\" START=\"53.759780\" LEN=\"0.000000\" "
            "REPEATS=\"-1\" HOTCUE=\"-1\"><GRID BPM=\"137.004074\"></GRID>\n</CUE_V2>\n"
            "<CUE_V2 NAME=\"n.n.\" DISPL_ORDER=\"0\" TYPE=\"0\" START=\"53.759780\" LEN=\"0.000000\" "
            "REPEATS=\"-1\" HOTCUE=\"0\"></CUE_V2>\n</ENTRY>\n")));
}

TEST(NmlWriterTest, NoBomNoCarriageReturnNoSelfClosedElements) {
    NmlWriteDocument doc;
    doc.entries = {entryFor(QStringLiteral("C:/a/x.mp3"))};
    doc.playlistOrder = {0};
    const QByteArray bytes = writeNml(doc);
    EXPECT_TRUE(bytes.startsWith("<?xml"));
    EXPECT_FALSE(bytes.contains('\r'));
    EXPECT_FALSE(bytes.contains("/>"));
    EXPECT_FALSE(bytes.contains("\n "));
}

TEST(NmlWriterTest, SplitsPathsIntoVolumeAndDir) {
    NmlLocation win = locationFromPath(QStringLiteral("C:/Users/J/My Music/a.mp3"));
    EXPECT_EQ(QStringLiteral("C:"), win.volume);
    EXPECT_EQ(QStringLiteral("/:Users/:J/:My Music/:"), win.dir);
    EXPECT_EQ(QStringLiteral("a.mp3"), win.file);
    EXPECT_EQ(QStringLiteral("C:/:Users/:J/:My Music/:a.mp3"), win.primaryKey());

    NmlLocation mac = locationFromPath(QStringLiteral("/Volumes/Data/DJ/b.flac"));
    EXPECT_EQ(QStringLiteral("Data"), mac.volume);
    EXPECT_EQ(QStringLiteral("/:DJ/:"), mac.dir);
}

TEST(NmlWriterTest, FormatsDatesAndFloats) {
    EXPECT_EQ(QStringLiteral("2021/4/16"), nmlReleaseDate(QStringLiteral("2021-04-16")));
    EXPECT_EQ(QStringLiteral("2021/1/1"), nmlReleaseDate(QStringLiteral("2021")));
    EXPECT_EQ(QStringLiteral("2021/4/16"), nmlReleaseDate(QStringLiteral("2021/4/16")));
    EXPECT_TRUE(nmlReleaseDate(QStringLiteral("spring")).isEmpty());
    EXPECT_EQ(QStringLiteral("136.000000"), nmlFloat(136.0));
}

TEST(NmlExportPathTest, DefaultPathAndRememberedFolder) {
    EXPECT_EQ(QStringLiteral("/exports/Warm Up/Warm Up.nml"),
            nmlDefaultExportPath(QStringLiteral("/exports"), QStringLiteral("Warm Up")));
    EXPECT_EQ(QStringLiteral("/exports/a_b/a_b.nml"),
            nmlDefaultExportPath(QStringLiteral("/exports"), QStringLiteral("a/b")));
    EXPECT_EQ(QStringLiteral("C:/exports"),
            nmlFolderToRemember(QStringLiteral("C:/exports/Warm Up/Warm Up.nml"),
                    QStringLiteral("Warm Up")));
    EXPECT_EQ(QStringLiteral("C:/somewhere/else"),
            nmlFolderToRemember(QStringLiteral("C:/somewhere/else/x.nml"),
                    QStringLiteral("Warm Up")));
    EXPECT_EQ(QStringLiteral("01 - Intro 1.flac"),
            nmlNumberedFileName(QStringLiteral("01 - Intro.flac"), 1));
}

class NmlExportWorkerTest : public testing::Test {
  protected:
    QString sourcePath(const QString& dir, const QString& name, const QByteArray& content) {
        const QString path = m_sources.filePath(dir + QLatin1Char('/') + name);
        writeFile(path, content);
        return path;
    }
    QString exportPath() const {
        return QDir(m_export.path()).filePath(QStringLiteral("Preparation/Preparation.nml"));
    }
    QString exportDir() const {
        return QDir(m_export.path()).filePath(QStringLiteral("Preparation"));
    }

    QTemporaryDir m_sources;
    QTemporaryDir m_export;
};

TEST_F(NmlExportWorkerTest, CopiesFlatUnderOriginalNamesAndPointsAtTheCopies) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    const QString b = sourcePath(QStringLiteral("y/deep"), QStringLiteral("two.mp3"), "bbbbbb");
    const NmlExportResult r = runExport(jobFor(exportPath(), {entryFor(a), entryFor(b)}));
    EXPECT_TRUE(r.error.isEmpty());
    EXPECT_EQ(2, r.copied);
    EXPECT_TRUE(QFile::exists(QDir(exportDir()).filePath(QStringLiteral("one.flac"))));
    EXPECT_TRUE(QFile::exists(QDir(exportDir()).filePath(QStringLiteral("two.mp3"))));

    NmlDocument doc;
    ASSERT_TRUE(doc.readFile(exportPath()));
    ASSERT_EQ(2, doc.entries.size());
    EXPECT_EQ(QDir(exportDir()).filePath(QStringLiteral("one.flac")),
            doc.entries.at(0).location.recordedPath());
}

TEST_F(NmlExportWorkerTest, CopyingOffWritesOnlyTheNmlPointingAtTheOriginals) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    const NmlExportResult r = runExport(jobFor(exportPath(), {entryFor(a)}, false));
    EXPECT_TRUE(r.error.isEmpty());
    EXPECT_EQ(0, r.copied);
    EXPECT_FALSE(QFile::exists(QDir(exportDir()).filePath(QStringLiteral("one.flac"))));
    NmlDocument doc;
    ASSERT_TRUE(doc.readFile(exportPath()));
    EXPECT_EQ(a, doc.entries.at(0).location.recordedPath());
}

TEST_F(NmlExportWorkerTest, ReExportSkipsIdenticalFilesAndDifferentClashesAreNumbered) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("01 - Intro.flac"), "aaaa");
    const QString b = sourcePath(QStringLiteral("y"), QStringLiteral("01 - Intro.flac"), "bbbbbb");
    const QList<NmlWriteEntry> items = {entryFor(a, QStringLiteral("A")), entryFor(b, QStringLiteral("B"))};

    NmlExportResult r = runExport(jobFor(exportPath(), items));
    EXPECT_EQ(2, r.copied);
    EXPECT_EQ(1, r.renamed);
    const QString first = QDir(exportDir()).filePath(QStringLiteral("01 - Intro.flac"));
    const QString second = QDir(exportDir()).filePath(QStringLiteral("01 - Intro 1.flac"));
    EXPECT_EQ(QByteArray("aaaa"), readAll(first));
    EXPECT_EQ(QByteArray("bbbbbb"), readAll(second));

    NmlDocument doc;
    ASSERT_TRUE(doc.readFile(exportPath()));
    EXPECT_EQ(first, doc.entries.at(0).location.recordedPath());
    EXPECT_EQ(second, doc.entries.at(1).location.recordedPath());

    // Again to the same folder: nothing is copied, references are the same.
    r = runExport(jobFor(exportPath(), items));
    EXPECT_EQ(0, r.copied);
    EXPECT_EQ(2, r.alreadyThere);
    NmlDocument again;
    ASSERT_TRUE(again.readFile(exportPath()));
    EXPECT_EQ(first, again.entries.at(0).location.recordedPath());
    EXPECT_EQ(second, again.entries.at(1).location.recordedPath());
}

TEST_F(NmlExportWorkerTest, SameSizeDifferentFilesWithTheSameNameStayApart) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("t.mp3"), "aaaa");
    const QString b = sourcePath(QStringLiteral("y"), QStringLiteral("t.mp3"), "bbbb");
    const NmlExportResult r = runExport(jobFor(exportPath(), {entryFor(a), entryFor(b)}));
    EXPECT_EQ(2, r.copied);
    EXPECT_TRUE(QFile::exists(QDir(exportDir()).filePath(QStringLiteral("t.mp3"))));
    EXPECT_TRUE(QFile::exists(QDir(exportDir()).filePath(QStringLiteral("t 1.mp3"))));
}

TEST_F(NmlExportWorkerTest, RepeatedTrackIsCopiedOnceAndKeepsItsPlaylistSlots) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    const QString b = sourcePath(QStringLiteral("x"), QStringLiteral("two.flac"), "bbbb");
    const NmlExportResult r =
            runExport(jobFor(exportPath(), {entryFor(a), entryFor(b), entryFor(a)}));
    EXPECT_EQ(2, r.copied);
    EXPECT_EQ(3, r.tracksInPlaylist);
    NmlDocument doc;
    ASSERT_TRUE(doc.readFile(exportPath()));
    EXPECT_EQ(2, doc.entries.size());
    ASSERT_EQ(3, doc.playlists.at(0).primaryKeys.size());
    EXPECT_EQ(doc.playlists.at(0).primaryKeys.at(0), doc.playlists.at(0).primaryKeys.at(2));
}

TEST_F(NmlExportWorkerTest, MissingTracksAreSkippedAndListed) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    NmlWriteEntry gone = entryFor(m_sources.filePath(QStringLiteral("gone.mp3")), QStringLiteral("Gone"));
    gone.artist = QStringLiteral("Nobody");
    const NmlExportResult r = runExport(jobFor(exportPath(), {gone, entryFor(a)}));
    EXPECT_TRUE(r.error.isEmpty());
    ASSERT_EQ(1, r.missing.size());
    EXPECT_EQ(QStringLiteral("Nobody - Gone"), r.missing.at(0));
    EXPECT_EQ(1, r.tracksInPlaylist);
    EXPECT_TRUE(nmlExportSummary(r).contains(QStringLiteral("Nobody - Gone")));
    NmlDocument doc;
    ASSERT_TRUE(doc.readFile(exportPath()));
    EXPECT_EQ(1, doc.entries.size());
}

TEST_F(NmlExportWorkerTest, CancelLeavesNoNml) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    NmlExportWorker worker(jobFor(exportPath(), {entryFor(a)}));
    worker.stop();
    worker.run();
    EXPECT_TRUE(worker.result().canceled);
    EXPECT_FALSE(QFile::exists(exportPath()));
}

TEST_F(NmlExportWorkerTest, ProgressIsReportedPerFile) {
    const QString a = sourcePath(QStringLiteral("x"), QStringLiteral("one.flac"), "aaaa");
    const QString b = sourcePath(QStringLiteral("x"), QStringLiteral("two.flac"), "bbbb");
    NmlExportWorker worker(jobFor(exportPath(), {entryFor(a), entryFor(b)}));
    QStringList seen;
    QObject::connect(&worker, &NmlExportWorker::progress, &worker,
            [&seen](const QString& name, int index, int count) {
                seen.append(QStringLiteral("%1 %2/%3").arg(name).arg(index).arg(count));
            },
            Qt::DirectConnection);
    worker.run();
    EXPECT_EQ((QStringList{QStringLiteral("one.flac 1/2"), QStringLiteral("two.flac 2/2")}), seen);
}

class NmlExportLibraryTest : public LibraryTest {};

TEST_F(NmlExportLibraryTest, ExportedNmlImportsBackWithTheSameTracksInTheSameOrder) {
    QTemporaryDir sources;
    QTemporaryDir exports;
    const QDir testDir = getTestDir();
    const QDir srcDir(sources.path());
    const QString flac = nmltest::copyAudio(testDir, QStringLiteral("cover-test.flac"), srcDir);
    const QString ogg = nmltest::copyAudio(testDir, QStringLiteral("cover-test.ogg"), srcDir);

    // A library Playlist: flac, ogg, flac again.
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    const int playlistId = dao.createPlaylist(QStringLiteral("Preparation"));
    ASSERT_NE(kInvalidPlaylistId, playlistId);
    const TrackPointer pFlac = getOrAddTrackByLocation(flac);
    const TrackPointer pOgg = getOrAddTrackByLocation(ogg);
    ASSERT_TRUE(pFlac && pOgg);
    ASSERT_TRUE(dao.appendTracksToPlaylist({pFlac->getId(), pOgg->getId(), pFlac->getId()}, playlistId));

    const QList<NmlWriteEntry> items = nmlExportItemsForPlaylist(trackCollectionManager(), playlistId);
    ASSERT_EQ(3, items.size());
    EXPECT_EQ(pFlac->getTitle(), items.at(0).title);
    EXPECT_EQ(items.at(0).path, items.at(2).path);

    const QString nmlPath = QDir(exports.path()).filePath(QStringLiteral("Preparation/Preparation.nml"));
    const NmlExportResult exported = runExport(jobFor(nmlPath, items));
    ASSERT_TRUE(exported.error.isEmpty());
    EXPECT_EQ(2, exported.copied);

    // Import it into a second playlist.
    NmlImportPlan plan = planNmlImport(nmlPath, dao);
    ASSERT_TRUE(plan.error.isEmpty());
    ASSERT_EQ(3, plan.tracks.size());
    for (const NmlPlanTrack& t : std::as_const(plan.tracks)) {
        EXPECT_EQ(NmlTrackStatus::Found, t.status);
    }
    const NmlImportResult imported =
            applyNmlImport(plan, QStringLiteral("Reimported"), trackCollectionManager());
    ASSERT_TRUE(imported.error.isEmpty());
    EXPECT_EQ(3, imported.tracksInPlaylist);
    const QList<NmlWriteEntry> back =
            nmlExportItemsForPlaylist(trackCollectionManager(), imported.playlistId);
    ASSERT_EQ(3, back.size());
    EXPECT_EQ(items.at(0).title, back.at(0).title);
    EXPECT_EQ(items.at(1).title, back.at(1).title);
    EXPECT_EQ(back.at(0).path, back.at(2).path);
    EXPECT_NE(back.at(0).path, back.at(1).path);
}
