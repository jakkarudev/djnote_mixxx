#include "engine/controls/snappedseekcontrol.h"

#include <algorithm>

#include "control/controlobject.h"
#include "moc_snappedseekcontrol.cpp"
#include "track/track.h"

SnappedSeekControl::SnappedSeekControl(const QString& group, UserSettingsPointer pConfig)
        : EngineControl(group, pConfig),
          m_pCOSnappedSeek(new ControlObject(ConfigKey(group, "overview_snapped_seek"), false)),
          m_requestedBeats(0.0) {
    connect(m_pCOSnappedSeek,
            &ControlObject::valueChanged,
            this,
            &SnappedSeekControl::slotSnappedSeek,
            Qt::DirectConnection);
}

SnappedSeekControl::~SnappedSeekControl() {
    delete m_pCOSnappedSeek;
}

// called from an engine worker thread
void SnappedSeekControl::trackLoaded(TrackPointer pNewTrack) {
    trackBeatsUpdated(pNewTrack ? pNewTrack->getBeats() : mixxx::BeatsPointer());
}

void SnappedSeekControl::trackBeatsUpdated(mixxx::BeatsPointer pBeats) {
    m_pBeats = pBeats;
}

// called from the GUI thread
void SnappedSeekControl::slotSnappedSeek(double beats) {
    m_requestedBeats.store(beats);
}

// called from the engine thread
void SnappedSeekControl::process(const double dRate,
        mixxx::audio::FramePos currentPosition,
        const int iBufferSize) {
    Q_UNUSED(dRate);
    Q_UNUSED(iBufferSize);
    // currentPosition is where the next buffer starts, which is where the seek
    // queued here is processed: the jump is relative to the position it
    // replaces, however fast the deck plays.
    const double beats = m_requestedBeats.exchange(0.0);
    const mixxx::BeatsPointer pBeats = m_pBeats;
    if (beats == 0.0 || !pBeats) {
        return;
    }
    const mixxx::audio::FramePos trackEndPosition = frameInfo().trackEndPosition;
    if (!trackEndPosition.isValid()) {
        return;
    }
    mixxx::audio::FramePos seekPosition = pBeats->findNBeatsFromPosition(currentPosition, beats);
    if (!seekPosition.isValid()) {
        return;
    }
    // The layout in the overview never lands outside the track, and neither
    // does the engine, whatever the grid says before the first beat.
    seekPosition = std::clamp(seekPosition, mixxx::audio::FramePos(0.0), trackEndPosition);
    // seekExact bypasses Quantize, like a beat jump does. A loop is left
    // alone: it is handled as for any other seek.
    seekExact(seekPosition);
}
