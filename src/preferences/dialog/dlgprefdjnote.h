#pragma once

#include "preferences/dialog/dlgpreferencepage.h"
#include "library/keymatch/keymatchsettings.h"
#include "preferences/usersettings.h"
#include "widget/knobreadoutsettings.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;

// The DJNote preferences page: settings of DJNote features that have no
// upstream page to live on. Built in code (no .ui file).
//
// Knob adjustment readout group: each setting is one control, shown by
// showKnobReadoutSettings() and read back by knobReadoutSettingsFromControls().
// To add a setting, add its control in createKnobReadoutGroup(), connect its
// change signal to slotKnobReadoutControlChanged() and cover it in those two
// functions; reset then follows. Like the Waveforms page, every change is
// stored (and so applied) at once, without waiting for Apply.
//
// Key matching group: the same way, through createKeyMatchingGroup(),
// slotKeyMatchingControlChanged(), showKeyMatchingSettings() and
// keyMatchingSettingsFromControls().
class DlgPrefDJNote : public DlgPreferencePage {
    Q_OBJECT
  public:
    DlgPrefDJNote(QWidget* pParent, UserSettingsPointer pConfig);

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;

  private slots:
    void slotKnobReadoutControlChanged();
    void slotKeyMatchingControlChanged();

  private:
    void createKnobReadoutGroup();
    // Greys out (or not) the rows that mean nothing while the readout is off.
    void setKnobReadoutDetailsEnabled(bool enabled);
    void showKnobReadoutSettings(const mixxx::knobreadout::Settings& settings);
    mixxx::knobreadout::Settings knobReadoutSettingsFromControls() const;

    void createKeyMatchingGroup();
    // Greys out what means nothing in the current state: the checklist while
    // Highlight is Off, the other columns while Whole row is ticked.
    void updateKeyMatchingEnabled();
    void showKeyMatchingSettings(const mixxx::keymatch::Settings& settings);
    mixxx::keymatch::Settings keyMatchingSettingsFromControls() const;

    const UserSettingsPointer m_pConfig;
    QCheckBox* m_pKnobReadoutEnabled;
    QLabel* m_pKnobReadoutModeLabel;
    QComboBox* m_pKnobReadoutMode;
    QLabel* m_pKnobReadoutStyleLabel;
    QComboBox* m_pKnobReadoutStyle;
    // Set while showKnobReadoutSettings() fills the controls, so that showing
    // settings doesn't store them.
    bool m_showingKnobReadoutSettings = false;

    QComboBox* m_pKeyMatchingHighlight;
    QCheckBox* m_pKeyMatchingShiftedKey;
    QListWidget* m_pKeyMatchingColumns;
    QCheckBox* m_pKeyMatchingBold;
    // Same as m_showingKnobReadoutSettings, for the key matching group.
    bool m_showingKeyMatchingSettings = false;
};
