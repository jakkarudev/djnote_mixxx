#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "control/controlproxy.h"
#include "library/keymatch/keymatchsettings.h"
#include "preferences/usersettings.h"
#include "proto/keys.pb.h"

namespace mixxx {
namespace keymatch {

// How a candidate key relates to the reference deck's key (GLOSSARY.md,
// "Key matching"; tiers in ADR 0007).
enum class KeyMatchTier {
    None,
    Strict,
    Fuzzy,
    // Matches the reference deck's shifted key, not its stored key.
    ShiftedStrict,
    ShiftedFuzzy,
};

// The tier of `candidate` against `reference`, ignoring settings and the
// shifted key. None when either key is invalid.
KeyMatchTier tierBetween(mixxx::track::io::key::ChromaticKey reference,
        mixxx::track::io::key::ChromaticKey candidate);

// Answers the key match of a candidate key against the reference deck: the
// playing sync leader; otherwise the loudest playing deck; otherwise the
// stopped sync leader; otherwise none. Decks only. It observes the deck
// controls and PlayerInfo's current playing deck signal, needs no loaded
// tracks and no audio engine.
//
// The Library owns the one instance the track list models consult
// (instance()); it is null when no matcher exists.
class KeyMatcher : public QObject {
    Q_OBJECT
  public:
    explicit KeyMatcher(const UserSettingsPointer& pConfig, QObject* pParent = nullptr);
    ~KeyMatcher() override;

    static KeyMatcher* instance();

    // The match of a candidate key, with the settings applied.
    KeyMatchTier match(mixxx::track::io::key::ChromaticKey candidate) const;

    const Settings& settings() const {
        return m_settings;
    }

    // Re-reads the settings from the config; call after storing them.
    void reloadSettings();

  signals:
    // Some answer of match() may have changed.
    void changed();

  private:
    struct Deck {
        explicit Deck(int number);
        ControlProxy play;
        ControlProxy syncLeader;
        ControlProxy fileKey;
        ControlProxy key;
    };

    struct Reference {
        int deck = 0; // 1-based, 0 for none
        mixxx::track::io::key::ChromaticKey storedKey = mixxx::track::io::key::INVALID;
        mixxx::track::io::key::ChromaticKey shiftedKey = mixxx::track::io::key::INVALID;
        bool operator==(const Reference&) const = default;
    };

    void ensureDecks();
    Reference findReference() const;
    void update();
    void onLoudestDeckChanged(int deckIndex);

    const UserSettingsPointer m_pConfig;
    Settings m_settings;
    ControlProxy m_numDecks;
    std::vector<std::unique_ptr<Deck>> m_decks;
    // The loudest playing deck as PlayerInfo reports it, 1-based, 0 for none.
    int m_loudestDeck;
    Reference m_reference;
};

} // namespace keymatch
} // namespace mixxx
