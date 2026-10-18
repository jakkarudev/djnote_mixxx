#include <gtest/gtest.h>

#include <memory>

#include "preferences/dialog/dlgprefwaveform.h"
#include "test/mixxxtest.h"
#include "waveform/waveformwidgetfactory.h"

class DlgPrefWaveformTest : public MixxxTest {
  protected:
    void SetUp() override {
        WaveformWidgetFactory::createInstance();
        WaveformWidgetFactory::instance()->setConfig(config());
    }
    void TearDown() override {
        WaveformWidgetFactory::destroy();
    }
};

// The Phosphor controls are built while the dialog is constructed; showing the
// stored settings must not run before they exist (it crashed Mixxx at startup).
TEST_F(DlgPrefWaveformTest, ConstructsAndShowsThePhosphorSettings) {
    auto pDialog = std::make_unique<DlgPrefWaveform>(nullptr, config(), nullptr);
    EXPECT_TRUE(pDialog);
}
