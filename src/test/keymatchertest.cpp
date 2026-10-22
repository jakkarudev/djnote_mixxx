#include "library/keymatch/keymatcher.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <memory>

#include "control/controlobject.h"
#include "library/keymatch/keymatchsettings.h"
#include "mixer/playerinfo.h"
#include "test/keymatchtestutils.h"
#include "test/mixxxtest.h"
#include "track/keyutils.h"

// The key matcher (ticket #96), driven the way the app drives it: through the
// deck controls and PlayerInfo's current playing deck signal.
namespace {

using keymatchtest::DeckControls;
using keymatchtest::KeyMatchTestBed;
using mixxx::keymatch::Highlight;
using mixxx::keymatch::KeyMatcher;
using mixxx::keymatch::KeyMatchTier;
using namespace mixxx::track::io::key;

// Camelot spelling for readable tests: "8A" is A minor, "8B" is C major.
ChromaticKey camelot(int hour, bool major) {
    // 8B is C major (tonic 0); each hour is a fifth (7 semitones); the relative
    // minor's tonic is 3 semitones below the major's.
    const int tonic = (((hour - 8) * 7 - (major ? 0 : 3)) % 12 + 12) % 12;
    return KeyUtils::tonicToKey(tonic, major);
}

class KeyMatcherTest : public MixxxTest {
  protected:
    KeyMatcherTest()
            : m_bed(config(), 2, 1) {
    }

    DeckControls& deck(int number) {
        return m_bed.deck(number);
    }

    KeyMatcher& matcher() {
        return m_bed.matcher();
    }

    // Deck 1 plays as sync leader with the given stored key.
    void playLeaderIn(ChromaticKey key) {
        deck(1).setStoredKey(key);
        deck(1).setShiftedKey(key);
        deck(1).syncLeader.set(2);
        deck(1).play.set(1);
    }

    KeyMatchTier matchOf(const char* name, bool major) {
        const QString text = QString::fromLatin1(name);
        return matcher().match(camelot(text.chopped(1).toInt(), major));
    }

    void setHighlight(Highlight highlight) {
        m_bed.changeSettings([highlight](auto& settings) { settings.highlight = highlight; });
    }

    void setMatchShiftedKey(bool match) {
        m_bed.changeSettings([match](auto& settings) { settings.matchShiftedKey = match; });
    }

    // 8A stored, played two semitones up: shifted key B minor (10A).
    void playLeaderShiftedUp() {
        deck(1).setStoredKey(camelot(8, false));
        deck(1).setShiftedKey(KeyUtils::tonicToKey(
                (KeyUtils::keyToTonic(camelot(8, false)) + 2) % 12, false));
        deck(1).syncLeader.set(2);
        deck(1).play.set(1);
    }

    // PlayerInfo reports the loudest playing deck from a 2 s timer; the
    // matcher listens to its signal (0-based deck index, -1 for none).
    void loudestDeckIs(int deckNumber) {
        emit PlayerInfo::instance().currentPlayingDeckChanged(deckNumber - 1);
    }

    KeyMatchTestBed m_bed;
};

} // namespace

TEST_F(KeyMatcherTest, TestHelperSpellsCamelot) {
    EXPECT_EQ(A_MINOR, camelot(8, false));
    EXPECT_EQ(C_MAJOR, camelot(8, true));
    EXPECT_EQ(E_MINOR, camelot(9, false));
}

TEST_F(KeyMatcherTest, MinorReferenceTiers) {
    playLeaderIn(camelot(8, false));
    for (int hour : {7, 8, 9}) {
        EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(hour, false)))
                << hour << "A";
    }
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, true))); // 8B
    for (int hour : {1, 3, 6, 10}) {
        EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(hour, false)))
                << hour << "A";
    }
    // Diagonals and everything else.
    for (int hour : {7, 9}) {
        EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(hour, true))) << hour << "B";
    }
    for (int hour : {2, 4, 5, 11, 12}) {
        EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(hour, false))) << hour << "A";
    }
    for (int hour : {1, 2, 3, 4, 5, 6, 10, 11, 12}) {
        EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(hour, true))) << hour << "B";
    }
}

TEST_F(KeyMatcherTest, MajorReferenceTiers) {
    playLeaderIn(camelot(8, true));
    for (int hour : {7, 8, 9}) {
        EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(hour, true))) << hour << "B";
    }
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false))); // 8A
    for (int hour : {1, 3, 6, 10}) {
        EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(hour, true))) << hour << "B";
    }
    for (int hour : {7, 9}) {
        EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(hour, false))) << hour << "A";
    }
}

TEST_F(KeyMatcherTest, TiersWrapAroundTheCamelotWheel) {
    playLeaderIn(camelot(12, false));
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(1, false)));
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(11, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(2, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(10, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(5, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(7, false)));
}

TEST_F(KeyMatcherTest, InvalidKeysNeverMatch) {
    playLeaderIn(camelot(8, false));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(INVALID));
}

TEST_F(KeyMatcherTest, ReferenceWithoutKeyHighlightsNothing) {
    playLeaderIn(INVALID);
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(8, false)));
}

TEST_F(KeyMatcherTest, PlayingSyncLeaderIsTheReference) {
    deck(2).setStoredKey(camelot(4, false));
    deck(2).play.set(1);
    playLeaderIn(camelot(8, false));
    loudestDeckIs(2);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(4, false)));
}

TEST_F(KeyMatcherTest, LoudestPlayingDeckIsTheReferenceWithoutLeader) {
    deck(1).setStoredKey(camelot(8, false));
    deck(1).play.set(1);
    deck(2).setStoredKey(camelot(4, false));
    deck(2).play.set(1);
    loudestDeckIs(2);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(4, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(8, false)));
    loudestDeckIs(1);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false)));
}

TEST_F(KeyMatcherTest, StoppedExplicitLeaderYieldsToAPlayingDeck) {
    deck(1).setStoredKey(camelot(8, false));
    deck(1).syncLeader.set(2);
    deck(2).setStoredKey(camelot(4, false));
    deck(2).play.set(1);
    loudestDeckIs(2);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(4, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(8, false)));
}

TEST_F(KeyMatcherTest, StoppedSyncLeaderIsTheReferenceWhenNothingPlays) {
    deck(1).setStoredKey(camelot(8, false));
    deck(1).syncLeader.set(2);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false)));
}

TEST_F(KeyMatcherTest, NoLeaderAndNothingPlayingHighlightsNothing) {
    deck(1).setStoredKey(camelot(8, false));
    deck(2).setStoredKey(camelot(4, false));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(8, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(4, false)));
}

TEST_F(KeyMatcherTest, SamplersAreNeverTheReference) {
    // A sampler is a sync leader and plays, but it is not a deck.
    ControlObject samplerPlay(ConfigKey(QStringLiteral("[Sampler1]"), QStringLiteral("play")));
    ControlObject samplerLeader(
            ConfigKey(QStringLiteral("[Sampler1]"), QStringLiteral("sync_leader")));
    ControlObject samplerFileKey(
            ConfigKey(QStringLiteral("[Sampler1]"), QStringLiteral("file_key")));
    samplerFileKey.set(KeyUtils::keyToNumericValue(camelot(5, false)));
    samplerLeader.set(2);
    samplerPlay.set(1);
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(5, false)));

    deck(1).setStoredKey(camelot(8, false));
    deck(1).syncLeader.set(2);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(5, false)));
}

TEST_F(KeyMatcherTest, ShiftedKeyMatchesAreTheirOwnTiers) {
    // 8A (A minor) stored, played two semitones up: B minor, 10A.
    deck(1).setStoredKey(camelot(8, false));
    deck(1).setShiftedKey(KeyUtils::tonicToKey(
            (KeyUtils::keyToTonic(camelot(8, false)) + 2) % 12, false));
    deck(1).syncLeader.set(2);
    deck(1).play.set(1);
    // Strict to B minor (9A, 10A, 11A, 10B) or fuzzy to it (8A, 12A, 3A, 5A),
    // where the stored key (8A) has no tier.
    EXPECT_EQ(KeyMatchTier::ShiftedStrict, matcher().match(camelot(11, false)));
    EXPECT_EQ(KeyMatchTier::ShiftedStrict, matcher().match(camelot(10, true)));
    EXPECT_EQ(KeyMatchTier::ShiftedFuzzy, matcher().match(camelot(12, false)));
    // The stored tier wins: 9A is strict to both keys, 10A is only fuzzy to 8A.
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(9, false)));
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(7, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(10, false)));
}

TEST_F(KeyMatcherTest, ShiftedKeyEqualToTheStoredKeyAddsNothing) {
    playLeaderIn(camelot(8, false));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(11, false)));
}

TEST_F(KeyMatcherTest, StrictOnlyDropsFuzzyTiers) {
    deck(1).setStoredKey(camelot(8, false));
    deck(1).setShiftedKey(KeyUtils::tonicToKey(
            (KeyUtils::keyToTonic(camelot(8, false)) + 2) % 12, false));
    deck(1).syncLeader.set(2);
    deck(1).play.set(1);
    setHighlight(Highlight::StrictOnly);
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(8, false)));
    EXPECT_EQ(KeyMatchTier::ShiftedStrict, matcher().match(camelot(11, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(10, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(12, false)));
}

TEST_F(KeyMatcherTest, ShiftedKeyMatchingIsOnByDefault) {
    EXPECT_TRUE(matcher().settings().matchShiftedKey);
}

TEST_F(KeyMatcherTest, ShiftedKeyMatchingOffDropsOnlyShiftedTiers) {
    playLeaderShiftedUp();
    setMatchShiftedKey(false);
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(11, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(12, false)));
    EXPECT_EQ(KeyMatchTier::Strict, matcher().match(camelot(9, false)));
    EXPECT_EQ(KeyMatchTier::Fuzzy, matcher().match(camelot(10, false)));
}

TEST_F(KeyMatcherTest, HighlightOffDropsShiftedMatchesToo) {
    playLeaderShiftedUp();
    setHighlight(Highlight::Off);
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(11, false)));
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(9, false)));
}

TEST_F(KeyMatcherTest, SignalsWhenShiftedKeyMatchingChanges) {
    playLeaderShiftedUp();
    QSignalSpy spy(&matcher(), &KeyMatcher::changed);
    setMatchShiftedKey(false);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(KeyMatcherTest, HighlightOffMatchesNothing) {
    playLeaderIn(camelot(8, false));
    setHighlight(Highlight::Off);
    EXPECT_EQ(KeyMatchTier::None, matcher().match(camelot(8, false)));
}

TEST_F(KeyMatcherTest, SignalsWhenTheReferenceDeckChanges) {
    deck(1).setStoredKey(camelot(8, false));
    deck(2).setStoredKey(camelot(4, false));
    deck(2).play.set(1);
    QSignalSpy spy(&matcher(), &KeyMatcher::changed);
    loudestDeckIs(2);
    EXPECT_EQ(1, spy.count());
    deck(1).syncLeader.set(2);
    deck(1).play.set(1);
    EXPECT_EQ(2, spy.count());
}

TEST_F(KeyMatcherTest, SignalsWhenTheReferenceKeyChanges) {
    playLeaderIn(camelot(8, false));
    QSignalSpy spy(&matcher(), &KeyMatcher::changed);
    deck(1).setStoredKey(camelot(4, false));
    EXPECT_EQ(1, spy.count());
    deck(1).setShiftedKey(camelot(5, false));
    EXPECT_EQ(2, spy.count());
}

TEST_F(KeyMatcherTest, SignalsWhenTheSettingChanges) {
    playLeaderIn(camelot(8, false));
    QSignalSpy spy(&matcher(), &KeyMatcher::changed);
    setHighlight(Highlight::Off);
    EXPECT_EQ(1, spy.count());
}

TEST_F(KeyMatcherTest, DoesNotSignalForAChangeThatChangesNoAnswer) {
    playLeaderIn(camelot(8, false));
    QSignalSpy spy(&matcher(), &KeyMatcher::changed);
    deck(2).setStoredKey(camelot(4, false)); // not the reference
    deck(1).play.set(1);                     // unchanged
    EXPECT_EQ(0, spy.count());
}
