#pragma once

#include <atomic>

#include "engine/controls/enginecontrol.h"
#include "preferences/usersettings.h"
#include "track/beats.h"
#include "track/track_decl.h"

class ControlObject;

/// The engine side of the overview's snapped seek: a relative jump along the
/// beat grid. The overview sets the control `overview_snapped_seek` to the
/// number of beats to jump (negative for backwards).
///
/// The jump is computed from the play position the deck has when the seek is
/// processed, so it is exact while the deck plays. Like a beat jump it bypasses
/// Quantize, but unlike a beat jump it never moves a loop.
class SnappedSeekControl : public EngineControl {
    Q_OBJECT
  public:
    SnappedSeekControl(const QString& group, UserSettingsPointer pConfig);
    ~SnappedSeekControl() override;

    void process(const double dRate,
            mixxx::audio::FramePos currentPosition,
            const int iBufferSize) override;
    void trackLoaded(TrackPointer pNewTrack) override;
    void trackBeatsUpdated(mixxx::BeatsPointer pBeats) override;

  private slots:
    void slotSnappedSeek(double beats);

  private:
    ControlObject* m_pCOSnappedSeek;

    // The jump requested by the GUI thread and not yet queued as a seek.
    // Zero if none.
    std::atomic<double> m_requestedBeats;

    // m_pBeats is written from an engine worker thread
    mixxx::BeatsPointer m_pBeats;
};
