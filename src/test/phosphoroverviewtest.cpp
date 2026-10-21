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

// The widget shows a vivid colour whose coverage (alpha) follows the pixel's
// brightness: a column fades from the centre into the background at its edges.
TEST_F(PhosphorOverviewTest, DisplayImageFadesFaintPixelsAndKeepsBrightOnesOpaque) {
    QImage src(6, 1, QImage::Format_ARGB32);
    src.setPixel(0, 0, qRgba(60, 40, 40, 80));    // edge
    src.setPixel(1, 0, qRgba(30, 20, 20, 40));    // fainter edge
    src.setPixel(2, 0, qRgba(90, 60, 60, 120));   // centre
    src.setPixel(3, 0, qRgba(0, 0, 0, 255));      // the silence line
    src.setPixel(4, 0, qRgba(0, 0, 0, 0));        // outside the extent
    src.setPixel(5, 0, qRgba(200, 0, 0, 0));      // colour without alpha (red/blue)
    const QImage out = overviewDisplayImage(src);
    ASSERT_EQ(src.size(), out.size());
    EXPECT_EQ(QImage::Format_ARGB32_Premultiplied, out.format());
    EXPECT_LT(qAlpha(out.pixel(1, 0)), qAlpha(out.pixel(0, 0)));
    EXPECT_LT(qAlpha(out.pixel(0, 0)), qAlpha(out.pixel(2, 0)));
    EXPECT_EQ(255, qAlpha(out.pixel(2, 0)));
    EXPECT_GT(qAlpha(out.pixel(1, 0)), 0);
    EXPECT_EQ(qRgba(0, 0, 0, 255), out.pixel(3, 0));
    EXPECT_EQ(0u, out.pixel(4, 0));
    EXPECT_GT(qAlpha(out.pixel(5, 0)), 0);
    EXPECT_GT(qRed(out.pixel(5, 0)), 0);
}

// The on-screen colours (research 10): the display image of each fixture image
// matches the reference script within 2 levels per channel.
TEST_F(PhosphorOverviewTest, DisplayImageMatchesFixtures) {
    int compared = 0;
    for (const QString& file : fixtureFiles()) {
        const auto f = load(file);
        ASSERT_TRUE(f.ok) << file.toStdString();
        for (const auto& im : f.images) {
            const auto want = f.display.find(im.name);
            if (want == f.display.end()) {
                continue;
            }
            OverviewColumnsView view;
            view.levels = f.levels.data();
            view.density = f.density.data();
            view.samplesPerColumn = static_cast<int>(f.samplesPerColumn);
            view.columnsDone = static_cast<int>(im.columnsDone);
            const QImage src = overviewImage(view, toDrawSettings(im.settings), static_cast<int>(im.height));
            const QImage out = overviewDisplayImage(src);  // premultiplied
            ASSERT_EQ(src.size(), out.size());
            ASSERT_EQ(want->second.size(), static_cast<size_t>(out.width()) * out.height() * 4);
            int worst = 0;
            for (int y = 0; y < out.height(); ++y) {
                const uint8_t* got = out.constScanLine(y);
                const uint8_t* ref = &want->second[static_cast<size_t>(y) * out.width() * 4];
                for (int x = 0; x < out.width(); ++x) {
                    // The fixture is straight B, G, R, A; compare premultiplied.
                    const int a = ref[x * 4 + 3];
                    for (int ch = 0; ch < 3; ++ch) {
                        const int want = (ref[x * 4 + ch] * a + 127) / 255;
                        worst = std::max(worst, std::abs(int(got[x * 4 + ch]) - want));
                    }
                    worst = std::max(worst, std::abs(int(got[x * 4 + 3]) - a));
                }
            }
            EXPECT_LE(worst, 2) << file.toStdString() << " " << im.name.toStdString();
            ++compared;
        }
    }
    EXPECT_GE(compared, 40);
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

// A loud master with varied columns, for the progressive drawing tests.
struct LoudColumns {
    std::vector<uint8_t> levels = std::vector<uint8_t>(kOverviewColumns * 3);
    std::vector<uint8_t> density =
            std::vector<uint8_t>(static_cast<size_t>(kOverviewColumns) * kOverviewRows);
    LoudColumns() {
        uint32_t seed = 1;
        const auto next = [&seed] {
            seed = seed * 1664525 + 1013904223;
            return seed >> 24;
        };
        for (auto& v : levels) {
            v = static_cast<uint8_t>(next());
        }
        for (int c = 0; c < kOverviewColumns; ++c) {
            // Every 50th column digital silence, the rest a varied extent.
            const bool silent = c % 50 == 0;
            const int extent = silent ? 0 : 1 + static_cast<int>(next() % kOverviewCenterRow);
            for (int r = kOverviewCenterRow - extent; r <= kOverviewCenterRow + extent - 1; ++r) {
                density[static_cast<size_t>(c) * kOverviewRows + r] =
                        static_cast<uint8_t>(1 + next() % 255);
            }
            if (silent) {
                levels[c * 3] = levels[c * 3 + 1] = levels[c * 3 + 2] = 0;
                density[static_cast<size_t>(c) * kOverviewRows + kOverviewCenterRow] = 255;
            }
        }
    }
    OverviewColumnsView view(int columnsDone) const {
        OverviewColumnsView v;
        v.levels = levels.data();
        v.density = density.data();
        v.samplesPerColumn = 6000;
        v.columnsDone = columnsDone;
        return v;
    }
};

// Issue #84: drawn tick by tick as the analyzer progresses, the display image
// is the one drawn at once.
TEST_F(PhosphorOverviewTest, DrawColumnsProgressivelyMatchesTheFullImage) {
    const LoudColumns loud;
    const PhosphorOverviewSettings settings;
    constexpr int kHeight = 2 * 255;
    QImage image;
    int drawn = 0;
    for (int done : {19, 38, 700, 701, 1500, kOverviewColumns}) {
        drawOverviewColumns(&image, loud.view(done), settings, kHeight, drawn);
        drawn = done;
        const QImage want = overviewDisplayImage(overviewImage(loud.view(done), settings, kHeight));
        ASSERT_EQ(want.size(), image.size()) << done;
        ASSERT_EQ(want.format(), image.format()) << done;
        EXPECT_TRUE(want == image) << done;
    }
}

// Issue #84: a tick draws only the new columns. Redrawing the columns already
// drawn on every analyzer tick made the GUI thread drop frames.
TEST_F(PhosphorOverviewTest, DrawColumnsLeavesTheColumnsAlreadyDrawn) {
    const LoudColumns loud;
    const PhosphorOverviewSettings settings;
    constexpr int kHeight = 2 * 255;
    QImage image;
    drawOverviewColumns(&image, loud.view(1000), settings, kHeight, 0);
    // Mark the columns already drawn; the next tick must not touch them.
    constexpr QRgb kMark = 0x12345678;
    for (int y = 0; y < kHeight; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        std::fill(line, line + 1000, kMark);
    }
    drawOverviewColumns(&image, loud.view(1019), settings, kHeight, 1000);
    const QImage want = overviewDisplayImage(overviewImage(loud.view(1019), settings, kHeight));
    ASSERT_EQ(want.size(), image.size());
    for (int y = 0; y < kHeight; ++y) {
        const QRgb* got = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        const QRgb* ref = reinterpret_cast<const QRgb*>(want.constScanLine(y));
        for (int x = 0; x < kOverviewColumns; ++x) {
            ASSERT_EQ(x < 1000 ? kMark : ref[x], got[x]) << x << "," << y;
        }
    }
}

// A null or wrongly sized image is drawn in full, whatever the first column.
TEST_F(PhosphorOverviewTest, DrawColumnsStartsOverOnANewImage) {
    const LoudColumns loud;
    const PhosphorOverviewSettings settings;
    QImage image(10, 10, QImage::Format_ARGB32_Premultiplied);
    drawOverviewColumns(&image, loud.view(800), settings, 100, 500);
    EXPECT_TRUE(overviewDisplayImage(overviewImage(loud.view(800), settings, 100)) == image);
}

} // namespace
