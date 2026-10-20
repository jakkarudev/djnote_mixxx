#include "widget/knobreadoutoverlay.h"

#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QWidget>
#include <algorithm>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "controllers/controllerinputscope.h"
#include "effects/effectchain.h"
#include "effects/effectparameterslotbase.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "moc_knobreadoutoverlay.cpp"
#include "widget/controlwidgetconnection.h"
#include "widget/knobreadoutface.h"
#include "widget/knobreadouttext.h"
#include "widget/wbasewidget.h"
#include "widget/wknob.h"
#include "widget/wknobcomposed.h"

namespace {

const QString kOverlayObjectName = QStringLiteral("DJNoteKnobReadoutOverlay");
const QString kLabelObjectName = QStringLiteral("DJNoteKnobReadout");
// Marks the ControlProxy the overlay parents to each watched knob, so a knob
// is never watched twice.
const QString kWatchObjectName = QStringLiteral("DJNoteKnobReadoutWatch");

constexpr int kHideDelayMs = 1000;
// Gap between the bottom of the knob and the top of the label.
constexpr int kGapPx = 2;

// The control a knob displays: the skin parser's display connection is the
// last left-button or no-button connection.
const ControlParameterWidgetConnection* displayConnection(WBaseWidget* pWidget) {
    if (!pWidget->connections().isEmpty()) {
        return pWidget->connections().last();
    }
    if (!pWidget->leftConnections().isEmpty()) {
        return pWidget->leftConnections().last();
    }
    return nullptr;
}

// The label's look: under the knob, or on the knob face (no border, so the
// padding matches the one knobreadoutface.h fits with).
QString labelStyleSheet(int pixelSize, bool onFace) {
    const QString border = onFace
            ? QStringLiteral("none")
            : QStringLiteral("1px solid rgba(255, 255, 255, 60)");
    const QString padding = onFace
            ? QStringLiteral("%1px %2px")
                      .arg(mixxx::knobreadout::kFaceVerticalPaddingPx)
                      .arg(mixxx::knobreadout::kFaceHorizontalPaddingPx)
            : QStringLiteral("1px 4px");
    return QStringLiteral(
            "QLabel#%1 {"
            " color: #f0f0f0;"
            " background-color: rgba(16, 16, 16, 220);"
            " border: %2;"
            " border-radius: 3px;"
            " padding: %3;"
            " font-size: %4px;"
            "}")
            .arg(kLabelObjectName, border, padding, QString::number(pixelSize));
}

// Under the knob, the label uses the full-size font.
constexpr int kUnderKnobPixelSize = 11;

// The units hint of knob parameter knobIndex (0-based) of the effect in slot
// effectGroup of chain chainGroup, if that parameter is loaded.
std::optional<EffectManifestParameter::UnitsHint> loadedKnobParameterUnitsHint(
        const EffectsManager& effectsManager,
        const QString& chainGroup,
        const QString& effectGroup,
        unsigned int knobIndex) {
    const EffectChainPointer pChain = effectsManager.getEffectChain(chainGroup);
    if (!pChain) {
        return std::nullopt;
    }
    for (const EffectSlotPointer& pEffectSlot : pChain->getEffectSlots()) {
        if (!pEffectSlot || pEffectSlot->getGroup() != effectGroup) {
            continue;
        }
        const EffectParameterSlotBasePointer pParameterSlot =
                pEffectSlot->getEffectParameterSlot(
                        EffectManifestParameter::ParameterType::Knob, knobIndex);
        if (pParameterSlot && pParameterSlot->isLoaded()) {
            return pParameterSlot->getManifest()->unitsHint();
        }
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

namespace mixxx {

namespace knobreadout {

bool isScreenTurnEvent(const QEvent& event) {
    // The buttons upstream's KnobEventHandler drags with.
    constexpr Qt::MouseButtons kDragButtons = Qt::LeftButton | Qt::MiddleButton;
    switch (event.type()) {
    case QEvent::Wheel:
        return true;
    case QEvent::MouseMove: {
        // Hovering has no button down; with the right button down the knob
        // ignores the move.
        const Qt::MouseButtons buttons = static_cast<const QMouseEvent&>(event).buttons();
        return (buttons & kDragButtons) && !(buttons & Qt::RightButton);
    }
    case QEvent::MouseButtonRelease:
        return kDragButtons.testFlag(static_cast<const QMouseEvent&>(event).button());
    default:
        // Presses only start a drag or reset the knob (right click, double
        // click): not turning it.
        return false;
    }
}

} // namespace knobreadout

// static
void KnobReadoutOverlay::onSkinLoaded(QWidget* pMainWindow,
        QWidget* pSkinRoot,
        UserSettingsPointer pConfig,
        std::shared_ptr<EffectsManager> pEffectsManager) {
    if (!pMainWindow || !pSkinRoot || !pConfig) {
        return;
    }
    auto* pOverlay = pMainWindow->findChild<KnobReadoutOverlay*>(
            kOverlayObjectName, Qt::FindDirectChildrenOnly);
    if (!pOverlay) {
        pOverlay = new KnobReadoutOverlay(pMainWindow, pConfig, pEffectsManager);
    }
    pOverlay->watchKnobs(pSkinRoot);
}

KnobReadoutOverlay::KnobReadoutOverlay(QWidget* pMainWindow,
        UserSettingsPointer pConfig,
        std::shared_ptr<EffectsManager> pEffectsManager)
        : QObject(pMainWindow),
          m_pMainWindow(pMainWindow),
          m_pConfig(pConfig),
          m_pEffectsManager(pEffectsManager),
          m_pLabel(new QLabel(pMainWindow)) {
    setObjectName(kOverlayObjectName);

    m_pLabel->setObjectName(kLabelObjectName);
    m_pLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_pLabel->setAlignment(Qt::AlignCenter);
    setLabelStyleSheet(labelStyleSheet(kUnderKnobPixelSize, false));
    m_pLabel->hide();

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kHideDelayMs);
    connect(&m_hideTimer, &QTimer::timeout, m_pLabel, &QWidget::hide);
}

knobreadout::Settings KnobReadoutOverlay::settings() const {
    return knobreadout::loadSettings(m_pConfig);
}

knobreadout::KnobIdentity KnobReadoutOverlay::identify(const ConfigKey& key) const {
    knobreadout::KnobIdentity knob{key};
    // Effect parameter knobs: [<chain group without "]">_Effect<n>],parameter<m>,
    // e.g. [EffectRack1_EffectUnit1_Effect2],parameter3.
    static const QRegularExpression kEffectSlotGroup(
            QStringLiteral("^(\\[.*)_Effect(\\d+)\\]$"));
    static const QRegularExpression kKnobParameterItem(
            QStringLiteral("^parameter(\\d+)$"));
    const QRegularExpressionMatch groupMatch = kEffectSlotGroup.match(key.group);
    const QRegularExpressionMatch itemMatch = kKnobParameterItem.match(key.item);
    if (!groupMatch.hasMatch() || !itemMatch.hasMatch()) {
        return knob;
    }
    const auto pEffectsManager = m_pEffectsManager.lock();
    if (!pEffectsManager) {
        return knob;
    }
    // A parameter slot exists exactly when its "_loaded" control does; this
    // avoids the slot lookup's debug assertion on a number out of range.
    if (!ControlObject::exists(ConfigKey(key.group, key.item + QStringLiteral("_loaded")))) {
        return knob;
    }
    knob.unitsHint = loadedKnobParameterUnitsHint(*pEffectsManager,
            groupMatch.captured(1) + QStringLiteral("]"),
            key.group,
            itemMatch.captured(1).toUInt() - 1);
    return knob;
}

void KnobReadoutOverlay::watchKnobs(QWidget* pSkinRoot) {
    // The previous skin's knobs are gone, and their watches with them.
    m_hideTimer.stop();
    m_pLabel->hide();
    // findChildren also returns the effect parameter and meta knob subclasses.
    const auto knobs = pSkinRoot->findChildren<WKnob*>();
    for (WKnob* pKnob : knobs) {
        watchKnob(pKnob);
    }
    const auto composedKnobs = pSkinRoot->findChildren<WKnobComposed*>();
    for (WKnobComposed* pKnob : composedKnobs) {
        watchKnob(pKnob);
    }
    qDebug() << "KnobReadoutOverlay: found" << knobs.size() << "knobs and"
             << composedKnobs.size() << "composed knobs";
}

void KnobReadoutOverlay::watchKnob(QWidget* pKnob) {
    if (pKnob->findChild<ControlProxy*>(kWatchObjectName, Qt::FindDirectChildrenOnly)) {
        return;
    }
    auto* pBaseWidget = dynamic_cast<WBaseWidget*>(pKnob);
    if (!pBaseWidget) {
        return;
    }
    const ControlParameterWidgetConnection* pConnection = displayConnection(pBaseWidget);
    if (!pConnection) {
        return;
    }
    // Parented to the knob, so the watch dies with the knob on skin reload.
    auto* pControl = new ControlProxy(pConnection->getKey(), pKnob);
    pControl->setObjectName(kWatchObjectName);
    const QPointer<QWidget> pKnobGuard(pKnob);
    pControl->connectValueChanged(this, [this, pKnobGuard, pControl](double) {
        if (pKnobGuard) {
            onKnobControlChanged(pKnobGuard, pControl);
        }
    });
    // Controller input: the watch samples the controller's input scope in the
    // controller thread; the connection is queued to this (GUI) thread and
    // dies with pControl, i.e. with the knob.
    if (auto* pWatch = ControllerInputWatch::forKey(pConnection->getKey())) {
        connect(pWatch,
                &ControllerInputWatch::changedByController,
                pControl,
                [this, pKnobGuard, pControl](double) {
                    if (!pKnobGuard) {
                        return;
                    }
                    m_pControllerInputKnob = pKnobGuard;
                    onKnobControlChanged(pKnobGuard, pControl);
                    m_pControllerInputKnob.clear();
                });
    }
    pKnob->installEventFilter(this);
}

bool KnobReadoutOverlay::eventFilter(QObject* pWatched, QEvent* pEvent) {
    // Runs before the knob handles the event; the knob then sets its control
    // synchronously. The mark is cleared on the next event-loop pass.
    if (knobreadout::isScreenTurnEvent(*pEvent)) {
        m_pScreenInputKnob = qobject_cast<QWidget*>(pWatched);
        if (!m_screenInputClearQueued) {
            m_screenInputClearQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_pScreenInputKnob.clear();
                m_screenInputClearQueued = false;
            });
        }
    }
    return QObject::eventFilter(pWatched, pEvent);
}

bool KnobReadoutOverlay::isOwnInput(QWidget* pKnob) const {
    return m_pScreenInputKnob == pKnob || m_pControllerInputKnob == pKnob;
}

void KnobReadoutOverlay::onKnobControlChanged(QWidget* pKnob, ControlProxy* pControl) {
    const knobreadout::Settings readoutSettings = settings();
    if (!readoutSettings.enabled) {
        // Turned off while a readout was showing: take it down now.
        m_hideTimer.stop();
        m_pLabel->hide();
        return;
    }
    if (!isOwnInput(pKnob)) {
        return;
    }
    const knobreadout::KnobIdentity knob = identify(pControl->getKey());
    const knobreadout::KnobValue value{pControl->get(), pControl->getParameter()};
    knobreadout::Options options;
    options.mode = readoutSettings.mode;
    if (readoutSettings.style == knobreadout::Style::KnobFace) {
        options.compact = true;
        const QString compactText = knobreadout::text(knob, value, options);
        const std::optional<int> pixelSize = knobreadout::faceFontPixelSize(
                compactText, m_pLabel->font(), pKnob->size());
        if (pixelSize) {
            showReadout(pKnob, compactText, *pixelSize);
            return;
        }
        // Even the compact text doesn't fit: full text under the knob.
        options.compact = false;
    }
    showReadout(pKnob, knobreadout::text(knob, value, options));
}

void KnobReadoutOverlay::setLabelStyleSheet(const QString& styleSheet) {
    // Re-polishing on every change would be wasted work while a knob turns.
    if (m_pLabel->styleSheet() != styleSheet) {
        m_pLabel->setStyleSheet(styleSheet);
    }
}

void KnobReadoutOverlay::showReadout(
        QWidget* pKnob, const QString& text, std::optional<int> facePixelSize) {
    if (!pKnob->isVisible()) {
        return;
    }
    setLabelStyleSheet(facePixelSize
                    ? labelStyleSheet(*facePixelSize, true)
                    : labelStyleSheet(kUnderKnobPixelSize, false));
    m_pLabel->setText(text);
    m_pLabel->adjustSize();

    // Centred on the knob face, or centred just under the knob; kept inside
    // the main window.
    const QPoint anchor = m_pMainWindow->mapFromGlobal(pKnob->mapToGlobal(
            facePixelSize ? pKnob->rect().center()
                          : QPoint(pKnob->width() / 2, pKnob->height())));
    const int maxX = std::max(0, m_pMainWindow->width() - m_pLabel->width());
    const int maxY = std::max(0, m_pMainWindow->height() - m_pLabel->height());
    const int x = std::clamp(anchor.x() - m_pLabel->width() / 2, 0, maxX);
    const int top = facePixelSize ? anchor.y() - m_pLabel->height() / 2 : anchor.y() + kGapPx;
    const int y = std::clamp(top, 0, maxY);
    m_pLabel->move(x, y);
    m_pLabel->show();
    m_pLabel->raise();
    m_hideTimer.start();
}

} // namespace mixxx
