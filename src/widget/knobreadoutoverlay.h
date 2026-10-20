#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class QLabel;
class QWidget;
class ControlProxy;

namespace mixxx {

// The knob adjustment readout (see GLOSSARY.md): a small label shown just
// under a knob while it is being turned, hidden about 1 s after the last
// change.
//
// The overlay observes upstream knob widgets (WKnob, WKnobComposed and their
// effect subclasses) without editing them: an event filter on each knob tells
// on-screen input (mouse drag, wheel) apart from hovering, a
// ControllerInputWatch reports controller input (MIDI, HID, bulk), and a
// ControlProxy
// on the knob's connected control reports the changes. The label is a child of
// the main window, so the knob's parent does not clip it.
class KnobReadoutOverlay : public QObject {
    Q_OBJECT
  public:
    // The one hook into upstream: the main window calls this each time it
    // finishes loading a skin (first load and every reload). Creates the
    // overlay on first use and watches the knobs found under pSkinRoot.
    static void onSkinLoaded(QWidget* pMainWindow, QWidget* pSkinRoot);

    bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

  private:
    explicit KnobReadoutOverlay(QWidget* pMainWindow);

    void watchKnobs(QWidget* pSkinRoot);
    void watchKnob(QWidget* pKnob);
    // Whether a change of pKnob's control comes from the user turning it, on
    // screen or on a controller.
    bool isOwnInput(QWidget* pKnob) const;
    void onKnobControlChanged(QWidget* pKnob, ControlProxy* pControl);
    void showReadout(QWidget* pKnob, const QString& text);

    QWidget* const m_pMainWindow;
    QLabel* const m_pLabel;
    QTimer m_hideTimer;
    // The knob receiving on-screen input in the current event-loop pass.
    QPointer<QWidget> m_pScreenInputKnob;
    // The knob whose control a controller message just changed, set only
    // while that change is handled (see ControllerInputWatch).
    QPointer<QWidget> m_pControllerInputKnob;
};

} // namespace mixxx
