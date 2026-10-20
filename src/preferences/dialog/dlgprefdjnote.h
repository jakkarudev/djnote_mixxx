#pragma once

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/usersettings.h"
#include "widget/knobreadoutsettings.h"

class QCheckBox;

// The DJNote preferences page: settings of DJNote features that have no
// upstream page to live on. Built in code (no .ui file).
//
// Knob adjustment readout group: each setting is one control, shown by
// showKnobReadoutSettings() and read back by knobReadoutSettingsFromControls().
// To add a setting, add its control in createKnobReadoutGroup() and cover it
// in those two functions; apply, cancel and reset then follow.
class DlgPrefDJNote : public DlgPreferencePage {
    Q_OBJECT
  public:
    DlgPrefDJNote(QWidget* pParent, UserSettingsPointer pConfig);

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;

  private:
    void createKnobReadoutGroup();
    void showKnobReadoutSettings(const mixxx::knobreadout::Settings& settings);
    mixxx::knobreadout::Settings knobReadoutSettingsFromControls() const;

    const UserSettingsPointer m_pConfig;
    QCheckBox* m_pKnobReadoutEnabled;
};
