#pragma once

#include <QObject>

#include "preferences/configobject.h"

namespace mixxx {

/// Marks the time Mixxx spends handling one incoming controller message
/// (MIDI, HID or bulk), including the mapping script functions it calls.
/// Control changes made inside the scope are the user's own controller input;
/// changes made outside it (script timers, EQ reset on track load, sync,
/// engine updates) are not.
///
/// The scope is per thread: controllers run in their own thread, so the flag
/// is only meaningful at the moment a control is set, in the setting thread.
/// Use ControllerInputWatch to observe it from another thread.
class ControllerInputScope {
  public:
    ControllerInputScope();
    ~ControllerInputScope();
    ControllerInputScope(const ControllerInputScope&) = delete;
    ControllerInputScope& operator=(const ControllerInputScope&) = delete;

    /// Whether the calling thread is handling a controller message.
    static bool isActive();
};

/// Reports the changes of one control that were made inside a
/// ControllerInputScope. It samples the scope in the thread that sets the
/// control, so receivers in other threads (the GUI) can connect normally and
/// get the signal queued.
///
/// There is one watch per control, owned by the control itself: it lives as
/// long as anything can set the control, so a controller thread never calls
/// into a deleted watch.
class ControllerInputWatch : public QObject {
    Q_OBJECT
  public:
    /// The watch of the control `key`, created on first use. Returns nullptr
    /// if the control does not exist. Call from the GUI thread.
    static ControllerInputWatch* forKey(const ConfigKey& key);

  signals:
    /// Emitted in the setting thread when the control changed inside a
    /// ControllerInputScope.
    void changedByController(double value);

  private:
    explicit ControllerInputWatch(QObject* pControl);
    void slotValueChanged(double value);
};

} // namespace mixxx
