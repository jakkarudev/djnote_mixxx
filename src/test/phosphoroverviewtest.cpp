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

PhosphorOverviewSettings toDrawSettings(const pf::Settings& s) {
    PhosphorOverviewSettings d;
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

// Silence is judged from the stored audio, not from the gained levels.
TEST_F(PhosphorOverviewTest, SilenceIsAudioNotGain) {
    std::vector<uint8_t> levels(kOverviewColumns * 3, 0);
    std::vector<uint8_t> density(static_cast<size_t>(kOverviewColumns) * kOverviewRows, 0);
    // Column 0: digital silence (levels 0, one row at the center).
    density[kOverviewCenterRow] = 255;
    // Column 1: a loud column (rows 15..46) with levels.
    for (int r = 15; r <= 46; ++r) {
        density[kOverviewRows + r] = 200;
    }
    levels[3] = 100;
    levels[4] = 200;
    levels[5] = 50;
    OverviewColumnsView view;
    view.levels = levels.data();
    view.density = density.data();
    view.samplesPerColumn = 100;
    view.columnsDone = 2;
    const auto pixelAt = [](const QImage& img, int column, int row) {
        return img.pixel(column, row);
    };

    PhosphorOverviewSettings settings;
    QImage img = overviewImage(view, settings);
    EXPECT_EQ(qRgba(0, 0, 0, 255), pixelAt(img, 0, kOverviewCenterRow - 1));
    EXPECT_EQ(0u, pixelAt(img, 0, kOverviewCenterRow));
    EXPECT_NE(0u, pixelAt(img, 1, 30));

    // All gains at 0: the silent column is still the line; the loud column is
    // dark and transparent, not a black block.
    settings.gainLow = settings.gainMid = settings.gainHigh = 0.0;
    img = overviewImage(view, settings);
    EXPECT_EQ(qRgba(0, 0, 0, 255), pixelAt(img, 0, kOverviewCenterRow - 1));
    for (int r = 0; r < kOverviewRows; ++r) {
        EXPECT_EQ(0u, pixelAt(img, 1, r)) << r;
    }
}

// The widget shows the colour as is: the stripe's alpha (2 x green) would
// darken it a second time.
TEST_F(PhosphorOverviewTest, DisplayImageIsOpaqueInsideTheExtent) {
    QImage src(4, 1, QImage::Format_ARGB32);
    src.setPixel(0, 0, qRgba(10, 20, 30, 40));  // faint, low alpha
    src.setPixel(1, 0, qRgba(0, 0, 0, 255));    // the silence line
    src.setPixel(2, 0, qRgba(0, 0, 0, 0));      // outside the extent
    src.setPixel(3, 0, qRgba(200, 0, 0, 0));    // colour without alpha (red/blue)
    const QImage out = overviewDisplayImage(src);
    ASSERT_EQ(src.size(), out.size());
    EXPECT_EQ(QImage::Format_ARGB32_Premultiplied, out.format());
    EXPECT_EQ(qRgba(10, 20, 30, 255), out.pixel(0, 0));
    EXPECT_EQ(qRgba(0, 0, 0, 255), out.pixel(1, 0));
    EXPECT_EQ(0u, out.pixel(2, 0));
    EXPECT_EQ(qRgba(200, 0, 0, 255), out.pixel(3, 0));
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
    density[5 * kOverviewRows + 21] = 9; // 10 rows above the center
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
