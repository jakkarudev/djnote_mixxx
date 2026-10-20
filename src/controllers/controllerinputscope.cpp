#include "controllers/controllerinputscope.h"

#include "control/control.h"
#include "moc_controllerinputscope.cpp"

namespace {

// Nesting depth of ControllerInputScope in the current thread.
thread_local int t_scopeDepth = 0;

const QString kWatchObjectName = QStringLiteral("DJNoteControllerInputWatch");

} // namespace

namespace mixxx {

ControllerInputScope::ControllerInputScope() {
    ++t_scopeDepth;
}

ControllerInputScope::~ControllerInputScope() {
    --t_scopeDepth;
}

bool ControllerInputScope::isActive() {
    return t_scopeDepth > 0;
}

ControllerInputWatch* ControllerInputWatch::forKey(const ConfigKey& key) {
    const QSharedPointer<ControlDoublePrivate> pControl =
            ControlDoublePrivate::getControl(key, ControlFlag::NoAssertIfMissing);
    if (!pControl) {
        return nullptr;
    }
    auto* pWatch = pControl->findChild<ControllerInputWatch*>(
            kWatchObjectName, Qt::FindDirectChildrenOnly);
    if (!pWatch) {
        pWatch = new ControllerInputWatch(pControl.data());
        // Direct: the scope must be sampled in the thread that sets the
        // control. The watch is a child of the control, so it outlives every
        // emission of this signal.
        connect(pControl.data(),
                &ControlDoublePrivate::valueChanged,
                pWatch,
                &ControllerInputWatch::slotValueChanged,
                Qt::DirectConnection);
    }
    return pWatch;
}

ControllerInputWatch::ControllerInputWatch(QObject* pControl)
        : QObject(pControl) {
    setObjectName(kWatchObjectName);
}

void ControllerInputWatch::slotValueChanged(double value) {
    if (ControllerInputScope::isActive()) {
        emit changedByController(value);
    }
}

} // namespace mixxx
