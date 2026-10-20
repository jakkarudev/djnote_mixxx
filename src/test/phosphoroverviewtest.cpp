#include <gtest/gtest.h>

#include <QDir>
#include <algorithm>
#include <cstdlib>

#include "test/mixxxtest.h"
#include "test/phosphoroverviewfixture.h"
#include "waveform/phosphor/phosphoroverview.h"

namespace {

using namespace mixxx::phosphor;
namespace pf = phosphoroverviewfixture;

class PhosphorOverviewTest : public MixxxTest {
  protected:
    QStringList fixtureFiles() const {
        return QDir(getTestDir().filePath("phosphor")).entryList({"*.phov"}, QDir::Files, QDir::Name);
    }
    pf::Fixture load(const QString& name) const {
        return pf::load(getTestDir().filePath("phosphor/" + name));
    }
};

OverviewDrawSettings toDrawSettings(const pf::Settings& s) {
    OverviewDrawSettings d;
    d.brightness = s.brightness;
    d.levelExponent = s.levelExponent;
    d.densityExponent = s.densityExponent;
    d.gainLow = s.gainLow;
    d.gainMid = s.gainMid;
    d.gainHigh = s.gainHigh;
    return d;
}

// Image function seam: the fixtures' images, drawn from the stored bytes.
TEST_F(PhosphorOverviewTest, ImageMatchesFixtures) {
    const QStringList files = fixtureFiles();
    ASSERT_FALSE(files.isEmpty());
    int compared = 0;
    for (const QString& file : files) {
        const auto f = load(file);
        ASSERT_TRUE(f.ok) << file.toStdString();
        for (const auto& im : f.images) {
            OverviewColumnsView view;
            view.levels = f.levels.data();
            view.density = f.density.data();
            view.samplesPerColumn = static_cast<int>(f.samplesPerColumn);
            view.columnsDone = static_cast<int>(im.columnsDone);
            const QImage img = overviewImage(view, toDrawSettings(im.settings), static_cast<int>(im.height));
            ASSERT_EQ(kOverviewColumns, img.width());
            ASSERT_EQ(static_cast<int>(im.height), img.height());
            ASSERT_EQ(QImage::Format_ARGB32, img.format());
            int worst = 0;
            for (int y = 0; y < img.height(); ++y) {
                const uint8_t* got = img.constScanLine(y);
                const uint8_t* want = &im.pixels[static_cast<size_t>(y) * kOverviewColumns * 4];
                for (int i = 0; i < kOverviewColumns * 4; ++i) {
                    worst = std::max(worst, std::abs(int(got[i]) - int(want[i])));
                }
            }
            EXPECT_LE(worst, 2) << file.toStdString() << " " << im.name.toStdString();
            ++compared;
        }
    }
    EXPECT_GT(compared, 50);
}

TEST_F(PhosphorOverviewTest, ExtentIsTheOutermostNonEmptyRow) {
    std::vector<uint8_t> levels(kOverviewColumns * 3, 0);
    std::vector<uint8_t> density(static_cast<size_t>(kOverviewColumns) * kOverviewRows, 0);
    OverviewColumnsView view;
    view.levels = levels.data();
    view.density = density.data();
    view.samplesPerColumn = 100;
    view.columnsDone = kOverviewColumns;
    EXPECT_EQ(0, overviewExtentRows(view));
    density[5 * kOverviewRows + 21] = 9; // 10 rows above the centre
    density[5 * kOverviewRows + 40] = 9; // 10 rows below
    EXPECT_EQ(10, overviewExtentRows(view));
    density[7 * kOverviewRows + 0] = 1; // the top row: full scale
    EXPECT_EQ(31, overviewExtentRows(view));
    view.columnsDone = 7; // column 7 is not built yet
    EXPECT_EQ(10, overviewExtentRows(view));
    density[9 * kOverviewRows + 61] = 1;
    view.columnsDone = kOverviewColumns;
    EXPECT_EQ(31, overviewExtentRows(view));
}

} // namespace
