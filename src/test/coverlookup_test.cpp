#include "library/coverlookup.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "test/librarytest.h"
#include "track/track.h"

// Readable gtest failure messages.
void PrintTo(const CoverInfo& coverInfo, std::ostream* pOut) {
    QString text;
    QDebug(&text) << coverInfo;
    *pOut << text.toStdString();
}

namespace {

const QString kEmbeddedPngTrack = QStringLiteral("id3-test-data/cover-test-png.mp3");
const QString kReferenceJpg = QStringLiteral("id3-test-data/cover_test.jpg");
const QString kReferencePng = QStringLiteral("id3-test-data/reference_cover.png");
const QString kNoCoverTrack = QStringLiteral("id3-test-data/empty.mp3");

class CoverLookupTest : public LibraryTest {
  protected:
    QString testFile(const QString& relativePath) const {
        return getTestDir().filePath(relativePath);
    }

    /// Asks for the cover of a file and waits until the lookup has read it.
    static CoverInfo coverInfoAfterReading(CoverLookup* pLookup, const QString& location) {
        QSignalSpy changed(pLookup, &CoverLookup::coverInfoChanged);
        pLookup->coverInfo(location);
        while (!changed.contains(QVariantList{location})) {
            if (!changed.wait(kReadTimeoutMillis)) {
                ADD_FAILURE() << "No cover read for " << location.toStdString();
                break;
            }
        }
        return pLookup->coverInfo(location);
    }

    int libraryTrackCount() const {
        QSqlQuery query(internalCollection()->database());
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM library")) || !query.next()) {
            ADD_FAILURE() << "Counting library tracks failed";
            return -1;
        }
        return query.value(0).toInt();
    }

    static constexpr int kReadTimeoutMillis = 5000;
};

TEST_F(CoverLookupTest, fileNotInLibraryAnswersEmbeddedCover) {
    const QString location = testFile(kEmbeddedPngTrack);
    CoverInfo embeddedCover;
    embeddedCover.type = CoverInfo::METADATA;
    embeddedCover.source = CoverInfo::GUESSED;
    embeddedCover.setImageDigest(QImage(testFile(kReferencePng)));
    embeddedCover.trackLocation = location;

    CoverLookup lookup(internalCollection());

    EXPECT_EQ(embeddedCover, coverInfoAfterReading(&lookup, location));
}

TEST_F(CoverLookupTest, fileNotInLibraryWithoutEmbeddedCoverAnswersFolderImage) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString location = folder.filePath(QStringLiteral("track.mp3"));
    const QString folderImage = folder.filePath(QStringLiteral("folder.jpg"));
    ASSERT_TRUE(QFile::copy(testFile(kNoCoverTrack), location));
    ASSERT_TRUE(QFile::copy(testFile(kReferenceJpg), folderImage));
    CoverInfo folderCover;
    folderCover.type = CoverInfo::FILE;
    folderCover.source = CoverInfo::GUESSED;
    folderCover.coverLocation = QStringLiteral("folder.jpg");
    folderCover.setImageDigest(QImage(folderImage));
    folderCover.trackLocation = location;

    CoverLookup lookup(internalCollection());

    EXPECT_EQ(folderCover, coverInfoAfterReading(&lookup, location));
}

TEST_F(CoverLookupTest, changingLibraryCoverChangesAnswer) {
    const TrackPointer pTrack = getOrAddTrackByLocation(testFile(kEmbeddedPngTrack));
    ASSERT_TRUE(pTrack);
    CoverLookup lookup(internalCollection());
    ASSERT_EQ(pTrack->getCoverInfoWithLocation(), lookup.coverInfo(pTrack->getLocation()));
    QSignalSpy changed(&lookup, &CoverLookup::coverInfoChanged);

    CoverInfo customCover;
    customCover.type = CoverInfo::FILE;
    customCover.source = CoverInfo::USER_SELECTED;
    customCover.coverLocation = testFile(kReferenceJpg);
    customCover.setImageDigest(QImage(customCover.coverLocation));
    customCover.trackLocation = pTrack->getLocation();
    pTrack->setCoverInfo(customCover);

    EXPECT_TRUE(changed.contains(QVariantList{pTrack->getLocation()}) ||
            changed.wait(kReadTimeoutMillis));
    EXPECT_EQ(customCover, lookup.coverInfo(pTrack->getLocation()));
}

TEST_F(CoverLookupTest, changingCoverOfTrackLoadedLaterChangesAnswer) {
    const QString location = testFile(kEmbeddedPngTrack);
    TrackId trackId;
    {
        const TrackPointer pTrack = getOrAddTrackByLocation(location);
        ASSERT_TRUE(pTrack);
        trackId = pTrack->getId();
    }
    CoverLookup lookup(internalCollection());
    ASSERT_TRUE(lookup.coverInfo(location).hasImage());
    QSignalSpy changed(&lookup, &CoverLookup::coverInfoChanged);

    // As when the track is opened in Track Info.
    const TrackPointer pTrack = trackCollectionManager()->getTrackById(trackId);
    ASSERT_TRUE(pTrack);
    CoverInfo customCover;
    customCover.type = CoverInfo::FILE;
    customCover.source = CoverInfo::USER_SELECTED;
    customCover.coverLocation = testFile(kReferenceJpg);
    customCover.setImageDigest(QImage(customCover.coverLocation));
    customCover.trackLocation = location;
    pTrack->setCoverInfo(customCover);

    EXPECT_TRUE(changed.contains(QVariantList{location}) ||
            changed.wait(kReadTimeoutMillis));
    EXPECT_EQ(customCover, lookup.coverInfo(location));
}

TEST_F(CoverLookupTest, libraryCoverWithoutDigestIsAnsweredWithDigest) {
    const QString location = testFile(kEmbeddedPngTrack);
    {
        const TrackPointer pTrack = getOrAddTrackByLocation(location);
        ASSERT_TRUE(pTrack);
        // A cover saved by old Mixxx versions only has a legacy hash.
        CoverInfoRelative legacyCover = pTrack->getCoverInfo();
        ASSERT_EQ(CoverInfo::METADATA, legacyCover.type);
        legacyCover.setImageDigest(QByteArray(), legacyCover.legacyHash());
        pTrack->setCoverInfo(legacyCover);
    }
    CoverInfo embeddedCover;
    embeddedCover.type = CoverInfo::METADATA;
    embeddedCover.source = CoverInfo::GUESSED;
    embeddedCover.setImageDigest(QImage(testFile(kReferencePng)));
    embeddedCover.trackLocation = location;

    CoverLookup lookup(internalCollection());
    const CoverInfo answer = lookup.coverInfo(location);
    // Never an image without digest: the cover column would add the track.
    EXPECT_FALSE(answer.hasImage() && answer.imageDigest().isEmpty());

    EXPECT_EQ(embeddedCover, coverInfoAfterReading(&lookup, location));
}

TEST_F(CoverLookupTest, fileAddedToLibraryAnswersLibraryCoverOnceChanged) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString location = folder.filePath(QStringLiteral("track.mp3"));
    ASSERT_TRUE(QFile::copy(testFile(kEmbeddedPngTrack), location));
    CoverLookup lookup(internalCollection());
    ASSERT_TRUE(coverInfoAfterReading(&lookup, location).hasImage());

    // As when a file selected in Computer is added, then edited in Track Info.
    const TrackPointer pTrack = getOrAddTrackByLocation(location);
    ASSERT_TRUE(pTrack);
    CoverInfo customCover;
    customCover.type = CoverInfo::FILE;
    customCover.source = CoverInfo::USER_SELECTED;
    customCover.coverLocation = testFile(kReferenceJpg);
    customCover.setImageDigest(QImage(customCover.coverLocation));
    customCover.trackLocation = location;
    pTrack->setCoverInfo(customCover);
    QCoreApplication::processEvents();

    EXPECT_EQ(customCover, lookup.coverInfo(location));
}

TEST_F(CoverLookupTest, missingFileAnswersEmptyCover) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString location = folder.filePath(QStringLiteral("missing.mp3"));
    // An image in the folder must not be picked for a file that is not there.
    ASSERT_TRUE(QFile::copy(testFile(kReferenceJpg), folder.filePath("folder.jpg")));

    CoverLookup lookup(internalCollection());

    EXPECT_FALSE(coverInfoAfterReading(&lookup, location).hasImage());
}

TEST_F(CoverLookupTest, missingLibraryTrackAnswersEmptyCover) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString location = folder.filePath(QStringLiteral("track.mp3"));
    ASSERT_TRUE(QFile::copy(testFile(kEmbeddedPngTrack), location));
    {
        const TrackPointer pTrack = getOrAddTrackByLocation(location);
        ASSERT_TRUE(pTrack);
        ASSERT_TRUE(pTrack->getCoverInfo().hasImage());
    }
    ASSERT_TRUE(QFile::remove(location));

    CoverLookup lookup(internalCollection());

    EXPECT_FALSE(lookup.coverInfo(location).hasImage());
}

TEST_F(CoverLookupTest, lookingUpFilesNotInLibraryAddsNoTracks) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString withFolderImage = folder.filePath(QStringLiteral("track.mp3"));
    ASSERT_TRUE(QFile::copy(testFile(kNoCoverTrack), withFolderImage));
    ASSERT_TRUE(QFile::copy(testFile(kReferenceJpg), folder.filePath("folder.jpg")));
    const int tracksBefore = libraryTrackCount();

    CoverLookup lookup(internalCollection());
    coverInfoAfterReading(&lookup, testFile(kEmbeddedPngTrack));
    coverInfoAfterReading(&lookup, withFolderImage);
    coverInfoAfterReading(&lookup, folder.filePath(QStringLiteral("missing.mp3")));

    EXPECT_EQ(tracksBefore, libraryTrackCount());
}

TEST_F(CoverLookupTest, libraryTrackAnswersLibraryCover) {
    const TrackPointer pTrack = getOrAddTrackByLocation(testFile(kEmbeddedPngTrack));
    ASSERT_TRUE(pTrack);
    const CoverInfo libraryCover = pTrack->getCoverInfoWithLocation();
    ASSERT_TRUE(libraryCover.hasImage());

    CoverLookup lookup(internalCollection());

    EXPECT_EQ(libraryCover, lookup.coverInfo(pTrack->getLocation()));
}

TEST_F(CoverLookupTest, libraryTrackAnswersCustomLibraryCover) {
    const QString location = testFile(kEmbeddedPngTrack);
    const QString customImageFile = testFile(kReferenceJpg);
    CoverInfo customCover;
    customCover.type = CoverInfo::FILE;
    customCover.source = CoverInfo::USER_SELECTED;
    customCover.coverLocation = customImageFile;
    customCover.setImageDigest(QImage(customImageFile));
    customCover.trackLocation = location;
    {
        TrackPointer pTrack = getOrAddTrackByLocation(location);
        ASSERT_TRUE(pTrack);
        pTrack->setCoverInfo(customCover);
        // Releasing the track saves it to the library.
    }

    CoverLookup lookup(internalCollection());

    EXPECT_EQ(customCover, lookup.coverInfo(location));
}

} // namespace
