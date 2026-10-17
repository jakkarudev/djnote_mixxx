#include <gtest/gtest.h>

#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/enginebuffer.h"
#include "test/mockedenginebackendtest.h"

namespace {

// 240 BPM at 44.1 kHz: a beat is 11025 frames. The fake tracks are 20 beats
// long.
constexpr double kBeatFrames = 11025.0;
constexpr double kBpm = 240.0;
// Frame positions are derived from the fraction of the track, hence the
// tolerance.
constexpr double kFrameTolerance = 1e-3;

} // namespace

class SnappedSeekControlTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        m_pTrack1->trySetBpm(kBpm);
        m_pTrack2->trySetBpm(kBpm);
        ProcessBuffer();
    }

    double position() {
        return m_pChannel1->getEngineBuffer()->getExactPlayPos().value();
    }

    void seekTo(double beats) {
        m_pChannel1->getEngineBuffer()->queueNewPlaypos(
                mixxx::audio::FramePos(beats * kBeatFrames), EngineBuffer::SEEK_EXACT);
        ProcessBuffer();
    }

    // The jump is computed at the end of the buffer being processed, from
    // the position the next buffer starts at, and queued as a seek.
    void requestSnappedSeek(double beats) {
        ControlObject::set(ConfigKey(m_sGroup1, "overview_snapped_seek"), beats);
        ProcessBuffer();
    }

    void snappedSeek(double beats) {
        requestSnappedSeek(beats);
        ProcessBuffer();
    }

    // How far deck 1 moves during one buffer while playing.
    double advancePerBuffer() {
        const double before = position();
        ProcessBuffer();
        return position() - before;
    }
};

TEST_F(SnappedSeekControlTest, LandsExactlyNBeatsLaterKeepingTheOffset) {
    seekTo(1.3);
    ASSERT_NEAR(1.3 * kBeatFrames, position(), kFrameTolerance);

    snappedSeek(8);
    EXPECT_NEAR(9.3 * kBeatFrames, position(), kFrameTolerance);

    snappedSeek(-4);
    EXPECT_NEAR(5.3 * kBeatFrames, position(), kFrameTolerance);
}

TEST_F(SnappedSeekControlTest, RequestingTheSameJumpTwiceJumpsTwice) {
    seekTo(1.3);
    snappedSeek(4);
    snappedSeek(4);
    EXPECT_NEAR(9.3 * kBeatFrames, position(), kFrameTolerance);
}

TEST_F(SnappedSeekControlTest, ZeroBeatsDoesNotMove) {
    seekTo(4.3);
    snappedSeek(0);
    EXPECT_NEAR(4.3 * kBeatFrames, position(), kFrameTolerance);
}

TEST_F(SnappedSeekControlTest, LandingPastTheEndIsClampedToTheEnd) {
    seekTo(15.3);
    snappedSeek(16);
    EXPECT_NEAR(ControlObject::get(ConfigKey(m_sGroup1, "track_samples")) / 2,
            position(),
            kFrameTolerance);
}

TEST_F(SnappedSeekControlTest, ExactWhilePlayingWithQuantizeAndAnotherDeckPlaying) {
    // Another deck plays, a quarter of a beat off phase, and deck 1 has
    // Quantize on. A standard seek would be moved to match the phase of
    // the other deck. A snapped seek is not.
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    m_pChannel2->getEngineBuffer()->queueNewPlaypos(
            mixxx::audio::FramePos(2.25 * kBeatFrames), EngineBuffer::SEEK_EXACT);
    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();
    // An exact seek is not phase adjusted: deck 1 is now out of phase.
    seekTo(4.3);
    const double advance = advancePerBuffer();
    ASSERT_GT(advance, 0.0);

    // The jump starts where the deck is when the request is processed, which
    // is where the next buffer starts, not where the request was made.
    requestSnappedSeek(8);
    const double jumpStart = position();
    ProcessBuffer();
    EXPECT_NEAR(jumpStart + 8 * kBeatFrames + advance, position(), kFrameTolerance);
}

TEST_F(SnappedSeekControlTest, ActiveLoopIsNotMoved) {
    // A loop of 4 beats, from beat 2 to beat 6. A beat jump inside a loop
    // would move the loop.
    seekTo(2.0);
    ControlObject::set(ConfigKey(m_sGroup1, "beatloop_size"), 4.0);
    ControlObject::set(ConfigKey(m_sGroup1, "beatloop_activate"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "beatloop_activate"), 0.0);
    ProcessBuffer();
    ASSERT_TRUE(ControlObject::toBool(ConfigKey(m_sGroup1, "loop_enabled")));
    const double loopStart = ControlObject::get(ConfigKey(m_sGroup1, "loop_start_position"));
    const double loopEnd = ControlObject::get(ConfigKey(m_sGroup1, "loop_end_position"));
    seekTo(2.5);

    snappedSeek(2);
    EXPECT_NEAR(4.5 * kBeatFrames, position(), kFrameTolerance);
    EXPECT_EQ(loopStart, ControlObject::get(ConfigKey(m_sGroup1, "loop_start_position")));
    EXPECT_EQ(loopEnd, ControlObject::get(ConfigKey(m_sGroup1, "loop_end_position")));
    EXPECT_TRUE(ControlObject::toBool(ConfigKey(m_sGroup1, "loop_enabled")));

    // Jumping out of the loop leaves the loop where it is and turns it off, as
    // any other seek does.
    snappedSeek(8);
    EXPECT_NEAR(12.5 * kBeatFrames, position(), kFrameTolerance);
    EXPECT_EQ(loopStart, ControlObject::get(ConfigKey(m_sGroup1, "loop_start_position")));
    EXPECT_EQ(loopEnd, ControlObject::get(ConfigKey(m_sGroup1, "loop_end_position")));
    EXPECT_FALSE(ControlObject::toBool(ConfigKey(m_sGroup1, "loop_enabled")));
}
