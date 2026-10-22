#pragma once

#include <gtest/gtest.h>

#include <QColor>
#include <QString>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "library/keymatch/keymatcher.h"
#include "library/keymatch/keymatchsettings.h"
#include "mixer/playerinfo.h"
#include "preferences/usersettings.h"
#include "track/keyutils.h"

// What the key matching tests share: the deck controls, a key matcher on top
// of PlayerInfo (the way the app drives them) and the tier colours. A test
// fixture owns one KeyMatchTestBed, declared before everything that uses the
// matcher so it is destroyed last.
namespace keymatchtest {

// The default tier colours.
inline const QColor kGreen(QStringLiteral("#4ade80"));
inline const QColor kYellow(QStringLiteral("#facc15"));

// The controls of one deck that key matching reads or drives.
class DeckControls {
  public:
    explicit DeckControls(int deck)
            : group(QStringLiteral("[Channel%1]").arg(deck)),
              play(ConfigKey(group, QStringLiteral("play"))),
              syncLeader(ConfigKey(group, QStringLiteral("sync_leader"))),
              fileKey(ConfigKey(group, QStringLiteral("file_key"))),
              key(ConfigKey(group, QStringLiteral("key"))) {
    }
    void setStoredKey(mixxx::track::io::key::ChromaticKey stored) {
        fileKey.set(KeyUtils::keyToNumericValue(stored));
    }
    void setShiftedKey(mixxx::track::io::key::ChromaticKey shifted) {
        key.set(KeyUtils::keyToNumericValue(shifted));
    }

    const QString group;
    ControlObject play;
    ControlObject syncLeader;
    ControlObject fileKey;
    ControlObject key;
};

class KeyMatchTestBed {
  public:
    KeyMatchTestBed(const UserSettingsPointer& pConfig, int numDecks, int numSamplers)
            : m_numDecks(ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks"))),
              m_numSamplers(ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_samplers"))),
              m_pConfig(pConfig) {
        m_numDecks.set(numDecks);
        m_numSamplers.set(numSamplers);
        PlayerInfo::create();
        for (int deck = 1; deck <= numDecks; ++deck) {
            m_decks.push_back(std::make_unique<DeckControls>(deck));
        }
        m_pMatcher = std::make_unique<mixxx::keymatch::KeyMatcher>(pConfig);
    }
    ~KeyMatchTestBed() {
        m_pMatcher.reset();
        PlayerInfo::destroy();
    }

    DeckControls& deck(int number) {
        return *m_decks.at(number - 1);
    }
    mixxx::keymatch::KeyMatcher& matcher() {
        return *m_pMatcher;
    }

    // The reference deck: deck 1 plays as sync leader in `camelot`.
    void referenceIs(const QString& camelot) {
        const auto key = KeyUtils::guessKeyFromText(camelot);
        ASSERT_NE(mixxx::track::io::key::INVALID, key);
        deck(1).setStoredKey(key);
        deck(1).setShiftedKey(key);
        deck(1).syncLeader.set(2);
        deck(1).play.set(1);
    }

    // The reference deck's file and shifted key both become 3B.
    void changeReferenceTo3B() {
        const auto key = KeyUtils::guessKeyFromText(QStringLiteral("3B"));
        deck(1).setStoredKey(key);
        deck(1).setShiftedKey(key);
    }

    // Stores the settings the way the preferences page does and applies them.
    void storeSettings(const mixxx::keymatch::Settings& settings) {
        mixxx::keymatch::saveSettings(m_pConfig, settings);
        m_pMatcher->reloadSettings();
    }

    // Changes one stored setting, keeping the rest.
    template<typename Change>
    void changeSettings(Change change) {
        mixxx::keymatch::Settings settings = mixxx::keymatch::loadSettings(m_pConfig);
        change(settings);
        storeSettings(settings);
    }

  private:
    ControlObject m_numDecks;
    ControlObject m_numSamplers;
    const UserSettingsPointer m_pConfig;
    std::vector<std::unique_ptr<DeckControls>> m_decks;
    std::unique_ptr<mixxx::keymatch::KeyMatcher> m_pMatcher;
};

} // namespace keymatchtest
