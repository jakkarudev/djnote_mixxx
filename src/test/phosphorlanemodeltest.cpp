#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "test/mixxxtest.h"
#include "test/phosphorfixture.h"
#include "waveform/phosphor/phosphorlanecache.h"
#include "waveform/phosphor/phosphorlanemodel.h"
#include "waveform/phosphor/phosphortexelcodec.h"

using namespace mixxx::phosphor;

namespace {

// The Full-detail (tier 1) lane cases of the fixtures, by the case names the
// ticket lists. Tier cases (N > 1) belong to the detail tier ticket.
const char* const kFixtures[] = {
    "q2a_630hz",
    "q4_silence",
    "q4_sine",
    "q4_square",
    "q5_click",
    "q5_kick",
    "q6_noise",
    "q7_inverted",
};

// Fixtures that carry decimated-tier lane cases.
const char* const kTierFixtures[] = {
    "q4_square",
    "q4_sine",
    "q6_noise",
    "q5_kick",
};

std::unique_ptr<PhosphorData> dataFromFixture(const phosphorfixture::Fixture& f, int blockSize = 1) {
    auto pData = std::make_unique<PhosphorData>(static_cast<int>(f.rate), f.frames, blockSize);
    std::vector<int16_t> mono(f.frames);
    for (uint32_t i = 0; i < f.frames; ++i) {
        const int l = f.input[i * f.channels];
        const int r = f.input[i * f.channels + (f.channels - 1)];
        mono[i] = static_cast<int16_t>(std::floor((l + r) / 2.0 + 0.5));
    }
    pData->append(mono.data(), static_cast<SINT>(mono.size()));
    pData->finish();
    return pData;
}

class PhosphorLaneModelTest : public MixxxTest {
  protected:
    phosphorfixture::Fixture load(const char* name) {
        auto f = phosphorfixture::load(
                getTestDir().filePath(QStringLiteral("phosphor/%1.phfx").arg(name)));
        EXPECT_TRUE(f.ok) << name;
        return f;
    }
};

TEST_F(PhosphorLaneModelTest, FullDetailColumnsMatchTheReferenceFixtures) {
    int casesChecked = 0;
    for (const char* name : kFixtures) {
        const auto f = load(name);
        const auto pData = dataFromFixture(f);
        const PhosphorData& data = *pData;
        for (const auto& lane : f.lanes) {
            if (lane.tierN != 1) {
                continue;
            }
            ++casesChecked;
            LaneParams params;
            params.framesPerColumn = framesPerColumn(f.rate, lane.pxPerBar, lane.bpm);
            params.height = static_cast<int>(lane.height);
            params.windowColumns = lane.windowMilliColumns / 1000.0;
            for (uint32_t x = 0; x < lane.width; ++x) {
                const double centre = lane.position +
                        (x + 0.5 - lane.playheadX) * params.framesPerColumn;
                const LaneColumn col = computeColumn(data, params, centre);
                const QString where = QStringLiteral("%1 / %2 / column %3")
                                              .arg(name, lane.name)
                                              .arg(x);
                ASSERT_EQ(lane.status[x], static_cast<uint8_t>(col.status)) << where.toStdString();
                if (col.status == ColumnStatus::Outside) {
                    continue;
                }
                ASSERT_EQ(lane.rowMin[x], col.rowMin) << where.toStdString();
                ASSERT_EQ(lane.rowMax[x], col.rowMax) << where.toStdString();
                ASSERT_EQ(lane.height, col.density.size()) << where.toStdString();
                for (uint32_t row = 0; row < lane.height; ++row) {
                    ASSERT_NEAR(lane.density[static_cast<size_t>(x) * lane.height + row],
                            col.density[row],
                            1.0 / 255.0)
                            << where.toStdString() << " / row " << row;
                }
            }
        }
    }
    // hollow square, sine (x2), noise, 630 Hz, clicks (x2), kick is tier 4
    // only, digital silence, L = -R
    EXPECT_GE(casesChecked, 9);
}

// The lane cases of the decimated tiers: the outline rows and the status are
// exact and the density is within 1/255, against the reference script.
TEST_F(PhosphorLaneModelTest, DecimatedTierColumnsMatchTheReferenceFixtures) {
    int casesChecked = 0;
    for (const char* name : kTierFixtures) {
        const auto f = load(name);
        for (const auto& lane : f.lanes) {
            if (lane.tierN == 1) {
                continue;
            }
            ++casesChecked;
            const auto pData = dataFromFixture(f, static_cast<int>(lane.tierN));
            LaneParams params;
            params.framesPerColumn = framesPerColumn(f.rate, lane.pxPerBar, lane.bpm);
            params.height = static_cast<int>(lane.height);
            params.windowColumns = lane.windowMilliColumns / 1000.0;
            for (uint32_t x = 0; x < lane.width; ++x) {
                const double centre = lane.position +
                        (x + 0.5 - lane.playheadX) * params.framesPerColumn;
                const LaneColumn col = computeColumn(*pData, params, centre);
                const QString where = QStringLiteral("%1 / %2 / column %3")
                                              .arg(name, lane.name)
                                              .arg(x);
                ASSERT_EQ(lane.status[x], static_cast<uint8_t>(col.status)) << where.toStdString();
                if (col.status == ColumnStatus::Outside) {
                    continue;
                }
                ASSERT_EQ(lane.rowMin[x], col.rowMin) << where.toStdString();
                ASSERT_EQ(lane.rowMax[x], col.rowMax) << where.toStdString();
                for (uint32_t row = 0; row < lane.height; ++row) {
                    ASSERT_NEAR(lane.density[static_cast<size_t>(x) * lane.height + row],
                            col.density[row],
                            1.0 / 255.0)
                            << where.toStdString() << " / row " << row;
                }
            }
        }
    }
    EXPECT_GE(casesChecked, 4);
}

// A tier keeps the outline: where a column spans several blocks, every tier's
// outline rows are those of Full, give or take one row. The only difference is
// at a column's window edge, where a block's extreme can lie just outside the
// window of points (hard square edges can lose more, so they are left out).
TEST_F(PhosphorLaneModelTest, OutlineRowsAtEveryTierStayWithinOneRowOfFull) {
    int columnsChecked = 0;
    for (const char* name : {"q4_sine", "q6_noise", "q5_kick"}) {
        const auto f = load(name);
        const auto pFull = dataFromFixture(f, 1);
        for (const int n : {4, 8, 16}) {
            const auto pTier = dataFromFixture(f, n);
            for (const auto& lane : f.lanes) {
                LaneParams params;
                params.framesPerColumn = framesPerColumn(f.rate, lane.pxPerBar, lane.bpm);
                params.height = static_cast<int>(lane.height);
                params.windowColumns = lane.windowMilliColumns / 1000.0;
                // Only views where a column spans several blocks.
                if (params.framesPerColumn < 2.0 * n) {
                    continue;
                }
                for (uint32_t x = 0; x < lane.width; ++x) {
                    const double centre = lane.position +
                            (x + 0.5 - lane.playheadX) * params.framesPerColumn;
                    const LaneColumn full = computeColumn(*pFull, params, centre);
                    const LaneColumn tier = computeColumn(*pTier, params, centre);
                    ASSERT_EQ(full.status, tier.status) << name << " N=" << n << " x=" << x;
                    if (full.status == ColumnStatus::Outside) {
                        continue;
                    }
                    ++columnsChecked;
                    ASSERT_NEAR(full.rowMin, tier.rowMin, 1) << name << " N=" << n << " x=" << x;
                    ASSERT_NEAR(full.rowMax, tier.rowMax, 1) << name << " N=" << n << " x=" << x;
                }
            }
        }
    }
    EXPECT_GT(columnsChecked, 100);
}

TEST_F(PhosphorLaneModelTest, DigitalSilenceIsFlagged) {
    const auto f = load("q4_silence");
    ASSERT_FALSE(f.lanes.empty());
    EXPECT_EQ(static_cast<uint8_t>(ColumnStatus::Silence), f.lanes[0].status[f.lanes[0].width / 2]);
}

TEST(PhosphorLaneModelPlainTest, FramesPerColumnUsesBarsAndFallsBackToBpm120) {
    // 217.4 px per bar at 120 BPM: a bar is 2 s.
    EXPECT_NEAR(2.0 * 44100 / 217.4, framesPerColumn(44100, 217.4, 120.0), 1e-9);
    EXPECT_DOUBLE_EQ(framesPerColumn(44100, 217.4, 120.0), framesPerColumn(44100, 217.4, 0.0));
    EXPECT_DOUBLE_EQ(framesPerColumn(44100, 217.4, 120.0), framesPerColumn(44100, 217.4, -3.0));
    // Twice the BPM, half the frames per column.
    EXPECT_NEAR(framesPerColumn(44100, 217.4, 120.0) / 2, framesPerColumn(44100, 217.4, 240.0), 1e-9);
}

TEST(PhosphorLaneModelPlainTest, ColumnsOutsideTheTrackAreOutside) {
    PhosphorData data(44100, 1000);
    const std::vector<int16_t> s(1000, 1000);
    data.append(s.data(), 1000);
    data.finish();
    LaneParams params;
    params.framesPerColumn = 10;
    params.height = 21;
    EXPECT_EQ(ColumnStatus::Outside, computeColumn(data, params, -500).status);
    EXPECT_EQ(ColumnStatus::Outside, computeColumn(data, params, 5000).status);
    EXPECT_EQ(ColumnStatus::Signal, computeColumn(data, params, 500).status);
}

TEST(PhosphorLaneModelPlainTest, ColumnsNeedDecodedData) {
    PhosphorData data(44100, 1000);
    const std::vector<int16_t> s(400, 1000);
    data.append(s.data(), 400);
    LaneParams params;
    params.framesPerColumn = 10;
    params.height = 21;
    EXPECT_TRUE(isColumnReady(data, params, 100));
    EXPECT_FALSE(isColumnReady(data, params, 399));
    EXPECT_FALSE(isColumnReady(data, params, 700));
    data.append(s.data(), 400);
    data.append(s.data(), 200);
    EXPECT_TRUE(isColumnReady(data, params, 999));
}

TEST(PhosphorTexelCodecTest, DensityRoundTripsAndSilenceMarksTheCentreRow) {
    LaneColumn column;
    column.status = ColumnStatus::Signal;
    column.density = {0.0f, 0.25f, 1.0f, 3.9f, 7.0f};
    std::vector<uint8_t> texels(5 * kBytesPerTexel);
    encodeColumn(&column, 5, texels.data());
    EXPECT_NEAR(0.25f, decodeDensity(&texels[1 * 4]), 1e-3);
    EXPECT_NEAR(3.9f, decodeDensity(&texels[3 * 4]), 1e-3);
    EXPECT_NEAR(kTexelDensityMax, decodeDensity(&texels[4 * 4]), 1e-3); // clipped
    for (int row = 0; row < 5; ++row) {
        EXPECT_EQ(0, texels[row * 4 + 1]) << row;
    }

    column.status = ColumnStatus::Silence;
    column.density.assign(5, 0.0f);
    encodeColumn(&column, 5, texels.data());
    for (int row = 0; row < 5; ++row) {
        EXPECT_EQ(row == rowOfValue(0, 5) ? 255 : 0, texels[row * 4 + 1]) << row;
    }

    encodeColumn(nullptr, 5, texels.data());
    for (const uint8_t b : texels) {
        EXPECT_EQ(0, b);
    }
}

// --- cache -----------------------------------------------------------------

std::shared_ptr<PhosphorData> noiseData() {
    auto data = std::make_shared<PhosphorData>(44100, 44100);
    std::vector<int16_t> s(44100);
    uint32_t state = 12345;
    for (auto& v : s) {
        state = state * 1664525u + 1013904223u;
        v = static_cast<int16_t>((state >> 16) & 0xFFFF);
    }
    data->append(s.data(), 44100);
    data->finish();
    return data;
}

bool sameColumn(const LaneColumn& a, const LaneColumn& b) {
    return a.status == b.status && a.rowMin == b.rowMin && a.rowMax == b.rowMax &&
            a.density == b.density;
}

TEST(PhosphorLaneCachePlainTest, ScrollingBackAndForthThenZoomingEqualsAFreshComputation) {
    const auto data = noiseData();
    LaneParams zoomedIn;
    zoomedIn.framesPerColumn = 7.5;
    zoomedIn.height = 41;
    LaneParams zoomedOut = zoomedIn;
    zoomedOut.framesPerColumn = 31.0;

    PhosphorLaneCache used(data, zoomedIn);
    // Scroll forward, then backward.
    for (int64_t first = 100; first < 400; first += 13) {
        used.retain(first, first + 63);
        for (int64_t k = first; k <= first + 63; ++k) {
            ASSERT_NE(nullptr, used.column(k));
        }
    }
    for (int64_t first = 400; first > 50; first -= 17) {
        used.retain(first, first + 63);
        for (int64_t k = first; k <= first + 63; ++k) {
            ASSERT_NE(nullptr, used.column(k));
        }
    }
    // Zoom.
    used.setParams(zoomedOut);
    used.retain(60, 123);
    PhosphorLaneCache fresh(data, zoomedOut);
    fresh.retain(60, 123);
    for (int64_t k = 60; k <= 123; ++k) {
        const LaneColumn* a = used.column(k);
        const LaneColumn* b = fresh.column(k);
        ASSERT_NE(nullptr, a);
        ASSERT_NE(nullptr, b);
        EXPECT_TRUE(sameColumn(*a, *b)) << "column " << k;
    }
    // And back to the first zoom: columns seen earlier come out the same.
    used.setParams(zoomedIn);
    used.retain(200, 263);
    PhosphorLaneCache fresh2(data, zoomedIn);
    for (int64_t k = 200; k <= 263; ++k) {
        ASSERT_TRUE(sameColumn(*used.column(k), *fresh2.column(k))) << "column " << k;
    }
}

TEST(PhosphorLaneCachePlainTest, ColumnsComputedWhileDecodingEqualThoseOfTheFinishedTrack) {
    auto data = std::make_shared<PhosphorData>(44100, 20000);
    std::vector<int16_t> s(20000);
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<int16_t>(20000 * std::sin(i * 0.05));
    }
    LaneParams params;
    params.framesPerColumn = 10;
    params.height = 41;
    PhosphorLaneCache cache(data, params);
    EXPECT_EQ(nullptr, cache.column(100)); // nothing decoded yet
    data->append(s.data(), 5000);
    ASSERT_NE(nullptr, cache.column(100));
    EXPECT_EQ(nullptr, cache.column(499)); // window reaches undecoded data
    data->append(s.data() + 5000, 15000);
    data->finish();
    PhosphorLaneCache fresh(data, params);
    for (int64_t k : {100, 499, 1000, 1999}) {
        ASSERT_NE(nullptr, cache.column(k));
        EXPECT_TRUE(sameColumn(*cache.column(k), *fresh.column(k))) << k;
    }
}

} // namespace
