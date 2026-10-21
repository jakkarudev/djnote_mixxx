#include <gtest/gtest.h>

#include <QString>

#include "library/traktor/traktorkey.h"
#include "proto/keys.pb.h"

namespace {

using namespace mixxx::track::io::key;
using mixxx::traktor::keyFromTraktorMusicalKeyValue;

TEST(TraktorKeyTest, MajorKeys) {
    EXPECT_EQ(C_MAJOR, keyFromTraktorMusicalKeyValue(QStringLiteral("0")));
    EXPECT_EQ(A_MAJOR, keyFromTraktorMusicalKeyValue(QStringLiteral("9")));
    EXPECT_EQ(B_MAJOR, keyFromTraktorMusicalKeyValue(QStringLiteral("11")));
}

TEST(TraktorKeyTest, MinorKeys) {
    EXPECT_EQ(C_MINOR, keyFromTraktorMusicalKeyValue(QStringLiteral("12")));
    EXPECT_EQ(A_MINOR, keyFromTraktorMusicalKeyValue(QStringLiteral("21")));
    EXPECT_EQ(B_MINOR, keyFromTraktorMusicalKeyValue(QStringLiteral("23")));
}

TEST(TraktorKeyTest, OutOfRangeIsInvalid) {
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("-1")));
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("24")));
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("100")));
}

TEST(TraktorKeyTest, MissingOrNonNumericIsInvalid) {
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QString()));
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("")));
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("abc")));
    EXPECT_EQ(INVALID, keyFromTraktorMusicalKeyValue(QStringLiteral("1.5")));
}

} // namespace
