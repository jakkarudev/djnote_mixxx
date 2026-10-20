#include <gtest/gtest.h>

#include <algorithm>

#include "test/mixxxtest.h"
#include "test/phosphoroverviewfixture.h"

namespace {

using phosphoroverviewfixture::kColumns;
using phosphoroverviewfixture::kRows;

class PhosphorOverviewFixtureTest : public MixxxTest {};

TEST_F(PhosphorOverviewFixtureTest, ReadsInputColumnsAndImages) {
    const auto f = phosphoroverviewfixture::load(
            getTestDir().filePath("phosphor/q4_sine_vs_square.phov"));
    ASSERT_TRUE(f.ok);
    EXPECT_EQ(44100u, f.rate);
    EXPECT_EQ(f.frames * f.channels, f.input.size());
    // The excerpt is a complete track: 1920 columns of ceil(frames / 1920) samples.
    EXPECT_EQ((f.frames + kColumns - 1) / kColumns, f.samplesPerColumn);
    ASSERT_EQ(static_cast<size_t>(kColumns) * 3, f.levels.size());
    ASSERT_EQ(static_cast<size_t>(kColumns) * kRows, f.density.size());

    ASSERT_FALSE(f.images.empty());
    const auto& im = f.images.front();
    EXPECT_EQ(QStringLiteral("default_h62"), im.name);
    EXPECT_EQ(62u, im.height);
    EXPECT_EQ(static_cast<uint32_t>(kColumns), im.columnsDone);
    EXPECT_EQ(1.0, im.settings.brightness);
    EXPECT_EQ(0.7, im.settings.levelExponent);
    EXPECT_EQ(static_cast<size_t>(62) * kColumns * 4, im.pixels.size());
    // The signal starts with silence: column 0 is a single-row line in the middle.
    int rows = 0;
    for (int r = 0; r < kRows; ++r) {
        rows += f.density[r] != 0;
    }
    EXPECT_EQ(1, rows);
    EXPECT_NE(0, f.density[31]);
    // ... and the sine that follows fills columns with bright rows.
    EXPECT_TRUE(std::any_of(im.pixels.begin(), im.pixels.end(), [](uint8_t v) { return v > 0; }));
}

} // namespace
