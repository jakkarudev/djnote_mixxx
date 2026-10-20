#include <gtest/gtest.h>

#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "widget/knobreadoutoverlay.h"

// Which knob events count as turning it on screen: a mouse drag or the wheel
// (spec #61). Resets (right click, double click) and hovering do not.
namespace {

using mixxx::knobreadout::isScreenTurnEvent;

QMouseEvent mouseEvent(QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
    return QMouseEvent(type, QPointF(5, 5), QPointF(105, 105), button, buttons, Qt::NoModifier);
}

TEST(KnobReadoutInputTest, DragWithLeftOrMiddleButtonTurnsTheKnob) {
    EXPECT_TRUE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton)));
    EXPECT_TRUE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::MiddleButton)));
    // The release ends the drag and sets the final value.
    EXPECT_TRUE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton)));
}

TEST(KnobReadoutInputTest, WheelTurnsTheKnob) {
    const QWheelEvent wheel(QPointF(5, 5),
            QPointF(105, 105),
            QPoint(),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
    EXPECT_TRUE(isScreenTurnEvent(wheel));
}

TEST(KnobReadoutInputTest, ResetsDoNotTurnTheKnob) {
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton)));
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseButtonDblClick, Qt::LeftButton, Qt::LeftButton)));
    // The knob ignores moves while the right button is down.
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::RightButton)));
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseButtonRelease, Qt::RightButton, Qt::NoButton)));
}

TEST(KnobReadoutInputTest, PressAndHoverDoNotTurnTheKnob) {
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton)));
    EXPECT_FALSE(isScreenTurnEvent(
            mouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton)));
    EXPECT_FALSE(isScreenTurnEvent(QEvent(QEvent::Enter)));
}

} // namespace
