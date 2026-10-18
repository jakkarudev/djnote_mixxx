#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "waveform/phosphor/phosphorlanecache.h"
#include "waveform/phosphor/phosphorlanemodel.h"

using namespace mixxx::phosphor;

namespace {

PhosphorDataPointer sineData() {
    auto pData = std::make_shared<PhosphorData>(44100, 20000);
    std::vector<int16_t> s(20000);
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<int16_t>(20000 * std::sin(i * 0.05));
    }
    pData->append(s.data(), static_cast<SINT>(s.size()));
    pData->finish();
    return pData;
}

} // namespace

TEST(PhosphorSlipOverlayTest, OverlayLaneIsHalfAsTallAndOtherwiseTheSameView) {
    LaneParams lane;
    lane.framesPerColumn = 12.5;
    lane.height = 101;
    lane.windowColumns = 2.0;
    const LaneParams overlay = slipOverlayParams(lane);
    EXPECT_EQ(50, overlay.height);
    EXPECT_DOUBLE_EQ(lane.framesPerColumn, overlay.framesPerColumn);
    EXPECT_DOUBLE_EQ(lane.windowColumns, overlay.windowColumns);

    lane.height = 1;
    EXPECT_EQ(1, slipOverlayParams(lane).height);
}

TEST(PhosphorSlipOverlayTest, OverlayShapeIsTheFullSignedShapeAtHalfScale) {
    const auto data = sineData();
    LaneParams lane;
    lane.framesPerColumn = 3.0;
    lane.height = 100;
    PhosphorLaneCache play(data, lane);
    PhosphorLaneCache slip(data, slipOverlayParams(lane));
    int checked = 0;
    for (int64_t k = 100; k < 140; ++k) {
        const LaneColumn* a = play.column(k);
        const LaneColumn* b = slip.column(k);
        ASSERT_NE(nullptr, a);
        ASSERT_NE(nullptr, b);
        ASSERT_EQ(ColumnStatus::Signal, b->status);
        // Same signed shape, half the rows: center at 25 of 50, not 50 of 100.
        EXPECT_NEAR(a->rowMin / 2.0, b->rowMin, 1.0) << k;
        EXPECT_NEAR(a->rowMax / 2.0, b->rowMax, 1.0) << k;
        EXPECT_EQ(50u, b->density.size());
        ++checked;
    }
    EXPECT_EQ(40, checked);
}

TEST(PhosphorSlipOverlayTest, SlipCacheIsIndependentOfThePlayCache) {
    const auto data = sineData();
    LaneParams lane;
    lane.framesPerColumn = 3.0;
    lane.height = 100;
    PhosphorLaneCache play(data, lane);
    PhosphorLaneCache slip(data, slipOverlayParams(lane));
    // Play and slip at different positions.
    for (int64_t k = 0; k < 200; ++k) {
        play.column(k);
    }
    for (int64_t k = 3000; k < 3100; ++k) {
        slip.column(k);
    }
    EXPECT_EQ(200u, play.cachedColumns());
    EXPECT_EQ(100u, slip.cachedColumns());
    EXPECT_FALSE(slip.contains(10));
    EXPECT_FALSE(play.contains(3050));
    PhosphorLaneCache fresh(data, slipOverlayParams(lane));
    EXPECT_EQ(fresh.column(3050)->density, slip.column(3050)->density);
}
