#pragma once

#include <QObject>

#include "analyzer/plugins/analyzerplugin.h"
#include "analyzer/plugins/analyzerqueenmarybeats.h"
#include "analyzer/plugins/analyzersoundtouchbeats.h"

namespace mixxx {

/// DJNote's jkr beat detectors: one of Mixxx's detectors finds the tempo, then
/// BeatGridRefiner places the constant grid on the kicks and the first
/// downbeat (see docs/adr/0004-beat-grid-refined-on-the-kick.md). They are
/// offered next to Mixxx's original detectors, which stay unchanged.
class AnalyzerJkrBeats {
  public:
    static AnalyzerPluginInfo queenMaryPluginInfo() {
        return AnalyzerPluginInfo(
                "jkr-qm-tempotracker",
                "DJNote",
                QObject::tr("jkr (Queen Mary tempo)"),
                true);
    }

    static AnalyzerPluginInfo soundTouchPluginInfo() {
        return AnalyzerPluginInfo(
                "jkr-soundtouch",
                "DJNote",
                QObject::tr("jkr (SoundTouch tempo)"),
                false);
    }

    static bool isJkr(const QString& pluginId) {
        return pluginId == queenMaryPluginInfo().id() ||
                pluginId == soundTouchPluginInfo().id();
    }

    /// The Mixxx detector that finds the tempo for a plugin id: itself for
    /// Mixxx's detectors.
    static QString tempoPluginId(const QString& pluginId) {
        if (pluginId == queenMaryPluginInfo().id()) {
            return AnalyzerQueenMaryBeats::pluginInfo().id();
        }
        if (pluginId == soundTouchPluginInfo().id()) {
            return AnalyzerSoundTouchBeats::pluginInfo().id();
        }
        return pluginId;
    }
};

} // namespace mixxx
