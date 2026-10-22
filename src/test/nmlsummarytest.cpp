#include <gtest/gtest.h>

#include "library/nml/nmlsummarydialog.h"

namespace mixxx {
namespace nml {

TEST(NmlSummaryTest, ShortSummaryIsAllHeadline) {
    const NmlSummaryParts parts = splitNmlSummary(QStringLiteral("Imported 3 tracks."));
    EXPECT_EQ(QStringLiteral("Imported 3 tracks."), parts.headline);
    EXPECT_TRUE(parts.details.isEmpty());
}

TEST(NmlSummaryTest, HeadlineIsTheFirstParagraphAndDetailsTheRest) {
    const NmlSummaryParts parts = splitNmlSummary(
            QStringLiteral("Exported 2 tracks.\n1 copied.\n\n1 missing:\na.mp3\n\n1 failed:\nb.mp3"));
    EXPECT_EQ(QStringLiteral("Exported 2 tracks.\n1 copied."), parts.headline);
    EXPECT_EQ(QStringLiteral("1 missing:\na.mp3\n\n1 failed:\nb.mp3"), parts.details);
}

TEST(NmlSummaryTest, EmptyTextGivesEmptyParts) {
    const NmlSummaryParts parts = splitNmlSummary(QString());
    EXPECT_TRUE(parts.headline.isEmpty());
    EXPECT_TRUE(parts.details.isEmpty());
}

TEST(NmlSummaryTest, DialogHeightIsCappedToTheScreen) {
    EXPECT_EQ(540, nmlSummaryMaxHeight(1080));
    EXPECT_EQ(300, nmlSummaryMaxHeight(400));
}

} // namespace nml
} // namespace mixxx
