#include "controllers/controllerinputscope.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include <memory>

#include "control/controlpotmeter.h"
#include "controllers/midi/legacymidicontrollermapping.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/scripting/legacy/controllerscriptenginelegacy.h"
#include "test/mixxxtest.h"
#include "util/time.h"

namespace {

class ControllerInputScopeTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_key = ConfigKey("[Channel1]", "pregain");
        // A gain knob: 0..4, mappable from MIDI.
        m_pControl = std::make_unique<ControlPotmeter>(m_key, 0.0, 4.0);
        auto* pWatch = mixxx::ControllerInputWatch::forKey(m_key);
        ASSERT_NE(nullptr, pWatch);
        // Receiver in the GUI (test) thread, like the knob readout overlay.
        QObject::connect(pWatch,
                &mixxx::ControllerInputWatch::changedByController,
                &m_receiver,
                [this](double value) { m_reported.push_back(value); });
    }

    // Runs `work` in a separate thread, as a controller does, and then
    // delivers what it queued to the GUI thread.
    void runInControllerThread(const std::function<void()>& work) {
        std::unique_ptr<QThread> pThread(QThread::create(work));
        pThread->start();
        pThread->wait();
        QCoreApplication::processEvents();
    }

    void setFromController(double value) {
        mixxx::ControllerInputScope scope;
        m_pControl->set(value);
    }

    ConfigKey m_key;
    std::unique_ptr<ControlPotmeter> m_pControl;
    QObject m_receiver;
    std::vector<double> m_reported;
};

TEST_F(ControllerInputScopeTest, ChangeWhileHandlingControllerInputIsReported) {
    runInControllerThread([this] { setFromController(0.5); });
    EXPECT_EQ(std::vector<double>{0.5}, m_reported);
}

TEST_F(ControllerInputScopeTest, ScriptTimerChangeInControllerThreadIsNotReported) {
    // A script timer runs in the controller thread, but outside the handling
    // of an incoming message.
    runInControllerThread([this] { m_pControl->set(0.25); });
    EXPECT_TRUE(m_reported.empty());
}

TEST_F(ControllerInputScopeTest, ProgrammaticChangeInGuiThreadIsNotReported) {
    // E.g. EQ reset on track load, sync, preferences.
    m_pControl->set(0.75);
    QCoreApplication::processEvents();
    EXPECT_TRUE(m_reported.empty());
}

TEST_F(ControllerInputScopeTest, ScopeOfAnotherThreadDoesNotCount) {
    // A controller handling a message must not make concurrent changes in
    // other threads look like controller input.
    mixxx::ControllerInputScope scopeInGuiThread;
    runInControllerThread([this] { m_pControl->set(0.25); });
    EXPECT_TRUE(m_reported.empty());
}

TEST_F(ControllerInputScopeTest, ChangeAfterTheMessageIsHandledIsNotReported) {
    runInControllerThread([this] {
        setFromController(0.5);
        m_pControl->set(0.25);
    });
    EXPECT_EQ(std::vector<double>{0.5}, m_reported);
}

TEST_F(ControllerInputScopeTest, NestedScopesEndWithTheOutermost) {
    runInControllerThread([this] {
        mixxx::ControllerInputScope outer;
        { mixxx::ControllerInputScope inner; }
        m_pControl->set(0.5);
    });
    EXPECT_EQ(std::vector<double>{0.5}, m_reported);
}

// A MIDI controller driven by the test through MidiController's own input
// path, so the scope hook in that path is what is under test.
class FakeMidiController : public MidiController {
  public:
    FakeMidiController()
            : MidiController("test") {
        startEngine();
        getScriptEngine()->initialize();
    }

    void receiveShortMessage(unsigned char status, unsigned char control, unsigned char value) {
        receivedShortMessage(status, control, value, mixxx::Time::elapsed());
    }

    void evaluate(const QString& code) {
        ASSERT_FALSE(getScriptEngine()->jsEngine()->evaluate(code).isError());
    }

    int open() override {
        return 0;
    }
    int close() override {
        return 0;
    }
    void sendShortMsg(unsigned char, unsigned char, unsigned char) override {
    }
    void sendBytes(const QByteArray&) override {
    }
    bool isPolling() const override {
        return false;
    }
};

class ControllerInputScopeMidiTest : public ControllerInputScopeTest {
  protected:
    void SetUp() override {
        ControllerInputScopeTest::SetUp();
        m_pMapping = std::make_shared<LegacyMidiControllerMapping>();
        m_pController = std::make_unique<FakeMidiController>();
    }

    void TearDown() override {
        m_pController.reset();
    }

    std::shared_ptr<LegacyMidiControllerMapping> m_pMapping;
    std::unique_ptr<FakeMidiController> m_pController;
};

TEST_F(ControllerInputScopeMidiTest, DirectBindingChangeIsReported) {
    constexpr unsigned char kStatus = 0xB0; // Control Change, channel 1
    constexpr unsigned char kControl = 0x10;
    m_pMapping->addInputMapping(MidiKey(kStatus, kControl).key,
            MidiInputMapping(MidiKey(kStatus, kControl), MidiOptions(), m_key));
    m_pController->setMapping(m_pMapping);

    m_pController->receiveShortMessage(kStatus, kControl, 0x7F);

    // CC value 0x7F maps to the maximum of the knob.
    EXPECT_EQ(std::vector<double>{4.0}, m_reported);
}

TEST_F(ControllerInputScopeMidiTest, ScriptHandlerChangeIsReported) {
    m_pController->setMapping(m_pMapping);
    m_pController->evaluate(
            "midi.makeInputHandler(0xB0, 0x11, (channel, control, value) => {"
            "  engine.setValue('[Channel1]', 'pregain', 0.5);"
            "})");

    m_pController->receiveShortMessage(0xB0, 0x11, 0x7F);

    EXPECT_EQ(std::vector<double>{0.5}, m_reported);
}

TEST_F(ControllerInputScopeMidiTest, ScriptTimerChangeIsNotReported) {
    m_pController->setMapping(m_pMapping);
    // The timer is started while handling a message but fires later.
    m_pController->evaluate(
            "midi.makeInputHandler(0xB0, 0x12, () => {"
            "  engine.beginTimer(20, () => {"
            "    engine.setValue('[Channel1]', 'pregain', 0.25);"
            "  }, true);"
            "})");
    m_pController->receiveShortMessage(0xB0, 0x12, 0x7F);

    QElapsedTimer waited;
    waited.start();
    while (m_pControl->get() != 0.25 && waited.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    ASSERT_EQ(0.25, m_pControl->get());
    EXPECT_TRUE(m_reported.empty());
}

TEST_F(ControllerInputScopeTest, OneWatchPerControl) {
    EXPECT_EQ(mixxx::ControllerInputWatch::forKey(m_key),
            mixxx::ControllerInputWatch::forKey(m_key));
    EXPECT_EQ(nullptr,
            mixxx::ControllerInputWatch::forKey(
                    ConfigKey("[Channel1]", "djnote_no_such_control")));
}

} // namespace
