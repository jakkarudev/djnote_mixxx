#include <gtest/gtest.h>

#include <QBuffer>
#include <QFile>

#include "library/nml/nmldocument.h"
#include "test/mixxxtest.h"

using namespace mixxx::nml;

namespace {

class NmlDocumentTest : public MixxxTest {
  protected:
    NmlDocument load(const QString& fixture) {
        NmlDocument doc;
        QString error;
        EXPECT_TRUE(doc.readFile(getTestDir().filePath(
                            QStringLiteral("nml-test-data/") + fixture),
                &error))
                << error.toStdString();
        return doc;
    }
};

TEST_F(NmlDocumentTest, ReadsVersion20Collection) {
    const NmlDocument doc = load(QStringLiteral("traktor-v20-playlist.nml"));
    EXPECT_EQ(20, doc.version);
    EXPECT_EQ(QStringLiteral("Traktor Pro 4"), doc.program);
    ASSERT_EQ(3, doc.entries.size());

    const NmlEntry& e = doc.entries.at(0);
    EXPECT_EQ(QStringLiteral("Connected"), e.title);
    EXPECT_EQ(QStringLiteral("Salona"), e.artist);
    EXPECT_EQ(QStringLiteral("Traktor Pro 4"), e.album);
    EXPECT_TRUE(e.locked);
    EXPECT_EQ(QStringLiteral("6m"), e.keyText);
    ASSERT_TRUE(e.musicalKey.has_value());
    EXPECT_EQ(20, *e.musicalKey);
    ASSERT_TRUE(e.bpm.has_value());
    EXPECT_DOUBLE_EQ(137.004074, *e.bpm);
    EXPECT_EQ(QStringLiteral("cover-test-vbr.mp3"), e.location.file);
    EXPECT_EQ(QStringLiteral("C:"), e.location.volume);
    EXPECT_EQ(QStringLiteral("C:/:nml-fixture-recorded/:cover-test-vbr.mp3"),
            e.location.primaryKey());
}

TEST_F(NmlDocumentTest, ReadsCuesWithPerMarkerGridBpm) {
    const NmlDocument doc = load(QStringLiteral("traktor-v20-playlist.nml"));
    const NmlEntry& e = doc.entries.at(0);
    ASSERT_EQ(4, e.cues.size());
    EXPECT_EQ(NmlCueType::Grid, e.cues.at(0).type);
    EXPECT_EQ(-1, e.cues.at(0).hotcue);
    EXPECT_DOUBLE_EQ(53.759780, e.cues.at(0).startMillis);
    ASSERT_TRUE(e.cues.at(0).gridBpm.has_value());
    EXPECT_DOUBLE_EQ(137.004074, *e.cues.at(0).gridBpm);
    EXPECT_EQ(NmlCueType::Cue, e.cues.at(1).type);
    EXPECT_EQ(0, e.cues.at(1).hotcue);
    EXPECT_FALSE(e.cues.at(1).gridBpm.has_value());
    ASSERT_TRUE(e.cues.at(2).gridBpm.has_value());
    EXPECT_DOUBLE_EQ(119.988373, *e.cues.at(2).gridBpm);
}

TEST_F(NmlDocumentTest, ReadsLoopOnButton) {
    const NmlDocument doc = load(QStringLiteral("traktor-v20-playlist.nml"));
    const NmlCue& loop = doc.entries.at(2).cues.at(1);
    EXPECT_EQ(NmlCueType::Loop, loop.type);
    EXPECT_EQ(6, loop.hotcue);
    EXPECT_DOUBLE_EQ(10739.366592, loop.startMillis);
    EXPECT_DOUBLE_EQ(7111.102262, loop.lengthMillis);
}

TEST_F(NmlDocumentTest, ReadsPlaylistWithRepeatsAndUuid) {
    const NmlDocument doc = load(QStringLiteral("traktor-v20-playlist.nml"));
    ASSERT_EQ(1, doc.playlists.size());
    const NmlPlaylist& pl = doc.playlists.at(0);
    EXPECT_EQ(QStringLiteral("Preparation"), pl.name);
    EXPECT_EQ(QStringLiteral("338613851b2e4bddb2926ac6a11ed148"), pl.uuid);
    EXPECT_TRUE(pl.folderPath.isEmpty());
    ASSERT_EQ(4, pl.primaryKeys.size());
    EXPECT_EQ(pl.primaryKeys.at(0), pl.primaryKeys.at(3));
    EXPECT_EQ(0, doc.entryIndexForPrimaryKey(pl.primaryKeys.at(3)));
    EXPECT_EQ(-1, doc.entryIndexForPrimaryKey(QStringLiteral("nope")));
}

TEST_F(NmlDocumentTest, ReadsVersion19WithoutGridChildren) {
    const NmlDocument doc = load(QStringLiteral("traktor-v19-playlist.nml"));
    EXPECT_EQ(19, doc.version);
    ASSERT_EQ(3, doc.entries.size());
    const NmlCue& grid = doc.entries.at(0).cues.at(0);
    EXPECT_EQ(NmlCueType::Grid, grid.type);
    EXPECT_EQ(0, grid.hotcue); // AutoGrid on button 1 in v19 files
    EXPECT_FALSE(grid.gridBpm.has_value());
    EXPECT_EQ(QStringLiteral("F#m"), doc.entries.at(0).keyText);
    EXPECT_FALSE(doc.entries.at(0).locked);
    ASSERT_EQ(1, doc.playlists.size());
    EXPECT_EQ(3, doc.playlists.at(0).primaryKeys.size());
}

TEST_F(NmlDocumentTest, ReadsFolderPathOfNestedPlaylist) {
    const QByteArray xml =
            "<NML VERSION=\"20\"><HEAD PROGRAM=\"x\"></HEAD><COLLECTION ENTRIES=\"0\"></COLLECTION>"
            "<PLAYLISTS><NODE TYPE=\"FOLDER\" NAME=\"$ROOT\"><SUBNODES COUNT=\"2\">"
            "<NODE TYPE=\"FOLDER\" NAME=\"Sets\"><SUBNODES COUNT=\"1\">"
            "<NODE TYPE=\"PLAYLIST\" NAME=\"Inner\"><PLAYLIST ENTRIES=\"0\" TYPE=\"LIST\" UUID=\"u1\">"
            "</PLAYLIST></NODE></SUBNODES></NODE>"
            "<NODE TYPE=\"PLAYLIST\" NAME=\"Top\"><PLAYLIST ENTRIES=\"0\" TYPE=\"LIST\" UUID=\"u2\">"
            "</PLAYLIST></NODE></SUBNODES></NODE></PLAYLISTS></NML>";
    QBuffer buffer;
    buffer.setData(xml);
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
    NmlDocument doc;
    ASSERT_TRUE(doc.read(&buffer));
    ASSERT_EQ(2, doc.playlists.size());
    EXPECT_EQ(QStringLiteral("Inner"), doc.playlists.at(0).name);
    EXPECT_EQ(QStringList{QStringLiteral("Sets")}, doc.playlists.at(0).folderPath);
    EXPECT_EQ(QStringLiteral("Top"), doc.playlists.at(1).name);
    EXPECT_TRUE(doc.playlists.at(1).folderPath.isEmpty());
}

TEST_F(NmlDocumentTest, RejectsNonNml) {
    QBuffer buffer;
    buffer.setData("<html></html>");
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
    NmlDocument doc;
    QString error;
    EXPECT_FALSE(doc.read(&buffer, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(NmlDocumentTest, ConvertsTraktorSeparators) {
    EXPECT_EQ(QStringLiteral("C:/Users/x/f.mp3"),
            fromTraktorSeparators(QStringLiteral("C:/:Users/:x/:f.mp3")));
}

} // namespace
