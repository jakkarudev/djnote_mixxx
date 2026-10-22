#include "library/keymatch/keymatcher.h"

#include "mixer/playerinfo.h"
#include "track/keyutils.h"
#include "util/assert.h"

#include "moc_keymatcher.cpp"

namespace mixxx {
namespace keymatch {

using namespace mixxx::track::io::key;

namespace {

KeyMatcher* s_pInstance = nullptr;

const QString kAppGroup = QStringLiteral("[App]");

QString deckGroup(int number) {
    return QStringLiteral("[Channel%1]").arg(number);
}

ChromaticKey keyFromControl(const ControlProxy& control) {
    if (!control.valid()) {
        return INVALID;
    }
    return KeyUtils::keyFromNumericValue(control.get());
}

} // namespace

KeyMatchTier tierBetween(ChromaticKey reference, ChromaticKey candidate) {
    if (reference == INVALID || candidate == INVALID) {
        return KeyMatchTier::None;
    }
    const bool major = KeyUtils::keyIsMajor(reference);
    const int referenceTonic = KeyUtils::keyToTonic(reference);
    const int candidateTonic = KeyUtils::keyToTonic(candidate);
    if (KeyUtils::keyIsMajor(candidate) != major) {
        // Only the relative key: the minor's tonic is 3 semitones below the
        // major's.
        const int minorTonic = major ? candidateTonic : referenceTonic;
        const int majorTonic = major ? referenceTonic : candidateTonic;
        return (majorTonic - minorTonic + 12) % 12 == 3 ? KeyMatchTier::Strict
                                                          : KeyMatchTier::None;
    }
    // Semitones apart, 0..11. A fifth (7) is one Camelot hour, and so is 5.
    const int distance = (candidateTonic - referenceTonic + 12) % 12;
    switch (distance) {
    case 0:
    case 5:
    case 7:
        return KeyMatchTier::Strict;
    case 1:
    case 2:
    case 10:
    case 11:
        return KeyMatchTier::Fuzzy;
    default:
        return KeyMatchTier::None;
    }
}

KeyMatcher::Deck::Deck(int number)
        : play(deckGroup(number), QStringLiteral("play")),
          syncLeader(deckGroup(number), QStringLiteral("sync_leader")),
          fileKey(deckGroup(number), QStringLiteral("file_key")),
          key(deckGroup(number), QStringLiteral("key")) {
}

KeyMatcher::KeyMatcher(const UserSettingsPointer& pConfig, QObject* pParent)
        : QObject(pParent),
          m_pConfig(pConfig),
          m_settings(loadSettings(pConfig)),
          m_numDecks(kAppGroup, QStringLiteral("num_decks"), this),
          m_loudestDeck(0) {
    DEBUG_ASSERT(!s_pInstance);
    s_pInstance = this;
    m_numDecks.connectValueChanged(this, [this](double) { update(); });
    PlayerInfo& playerInfo = PlayerInfo::instance();
    m_loudestDeck = playerInfo.getCurrentPlayingDeck() + 1;
    connect(&playerInfo,
            &PlayerInfo::currentPlayingDeckChanged,
            this,
            &KeyMatcher::onLoudestDeckChanged);
    // Deck controls may not exist yet when the matcher is created; a track
    // loading into a deck means they do.
    connect(&playerInfo, &PlayerInfo::trackChanged, this, [this] { update(); });
    ensureDecks();
    m_reference = findReference();
}

KeyMatcher::~KeyMatcher() {
    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }
}

// static
KeyMatcher* KeyMatcher::instance() {
    return s_pInstance;
}

void KeyMatcher::ensureDecks() {
    const int numDecks = m_numDecks.valid() ? static_cast<int>(m_numDecks.get()) : 0;
    for (int number = static_cast<int>(m_decks.size()) + 1; number <= numDecks; ++number) {
        m_decks.push_back(std::make_unique<Deck>(number));
        Deck& deck = *m_decks.back();
        for (ControlProxy* pControl :
                {&deck.play, &deck.syncLeader, &deck.fileKey, &deck.key}) {
            pControl->connectValueChanged(this, [this](double) { update(); });
        }
    }
}

KeyMatcher::Reference KeyMatcher::findReference() const {
    const int numDecks = static_cast<int>(m_decks.size());
    const auto isPlaying = [this](int number) {
        return m_decks[number - 1]->play.get() != 0.0;
    };
    const auto isLeader = [this](int number) {
        return m_decks[number - 1]->syncLeader.get() != 0.0;
    };

    int reference = 0;
    // 1. The playing sync leader.
    for (int number = 1; number <= numDecks && reference == 0; ++number) {
        if (isPlaying(number) && isLeader(number)) {
            reference = number;
        }
    }
    // 2. The loudest playing deck.
    if (reference == 0 && m_loudestDeck >= 1 && m_loudestDeck <= numDecks &&
            isPlaying(m_loudestDeck)) {
        reference = m_loudestDeck;
    }
    // 3. With nothing playing, the stopped sync leader.
    if (reference == 0) {
        bool anyPlaying = false;
        for (int number = 1; number <= numDecks; ++number) {
            anyPlaying = anyPlaying || isPlaying(number);
        }
        if (!anyPlaying) {
            for (int number = 1; number <= numDecks && reference == 0; ++number) {
                if (isLeader(number)) {
                    reference = number;
                }
            }
        }
    }

    Reference result;
    result.deck = reference;
    if (reference != 0) {
        const Deck& deck = *m_decks[reference - 1];
        result.storedKey = keyFromControl(deck.fileKey);
        const ChromaticKey shifted = keyFromControl(deck.key);
        result.shiftedKey = shifted == result.storedKey ? INVALID : shifted;
    }
    return result;
}

void KeyMatcher::update() {
    ensureDecks();
    const Reference reference = findReference();
    if (reference == m_reference) {
        return;
    }
    m_reference = reference;
    emit changed();
}

void KeyMatcher::onLoudestDeckChanged(int deckIndex) {
    m_loudestDeck = deckIndex + 1;
    update();
}

void KeyMatcher::reloadSettings() {
    m_settings = loadSettings(m_pConfig);
    emit changed();
}

KeyMatchTier KeyMatcher::match(ChromaticKey candidate) const {
    if (m_settings.highlight == Highlight::Off || m_reference.storedKey == INVALID) {
        return KeyMatchTier::None;
    }
    // The stored key's tier always wins over the shifted key's.
    KeyMatchTier tier = tierBetween(m_reference.storedKey, candidate);
    if (tier == KeyMatchTier::None && m_settings.matchShiftedKey &&
            m_reference.shiftedKey != INVALID) {
        switch (tierBetween(m_reference.shiftedKey, candidate)) {
        case KeyMatchTier::Strict:
            tier = KeyMatchTier::ShiftedStrict;
            break;
        case KeyMatchTier::Fuzzy:
            tier = KeyMatchTier::ShiftedFuzzy;
            break;
        default:
            break;
        }
    }
    if (m_settings.highlight == Highlight::StrictOnly &&
            (tier == KeyMatchTier::Fuzzy || tier == KeyMatchTier::ShiftedFuzzy)) {
        return KeyMatchTier::None;
    }
    return tier;
}

} // namespace keymatch
} // namespace mixxx
