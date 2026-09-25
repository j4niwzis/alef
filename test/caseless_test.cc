// Caseless matching, D144 to D146 of the Unicode Standard, by example: what
// each kind of match lets through and what it does not.
import std;
import alef.utf;
import alef.casing;
import alef.caseless;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"

CONSTEXPR_TEST(Caseless, CanonicalMatch) {
  // Case, full folding included: ß is ss.
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u8"Straße", u8"STRASSE"));
  // A precomposed letter and its decomposition: the default match misses
  // it, the canonical one does not.
  CONSTEXPR_EXPECT_FALSE(alef::equal_ignoring_case(u8"é", u8"É"));
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u8"é", u8"É"));
  // U+0345 inside a precomposed Greek letter, found by the first NFD.
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u8"ᾳ", u8"ΑΙ"));
  // Across UTFs.
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u"Å", U"å"));
  // Compatibility differences are not ignored.
  CONSTEXPR_EXPECT_FALSE(alef::equivalent_ignoring_case(u8"①", u8"1"));
  CONSTEXPR_EXPECT_FALSE(alef::equivalent_ignoring_case(u8"abc", u8"abd"));
}

CONSTEXPR_TEST(Caseless, CompatibilityMatch) {
  CONSTEXPR_EXPECT_TRUE(alef::compatible_ignoring_case(u8"①", u8"1"));
  CONSTEXPR_EXPECT_TRUE(alef::compatible_ignoring_case(u8"ｆｕｌｌ", u8"FULL"));
  CONSTEXPR_EXPECT_TRUE(alef::compatible_ignoring_case(u8"ﬃ", u8"FFI"));
  CONSTEXPR_EXPECT_TRUE(alef::compatible_ignoring_case(u8"Straße", u8"strasse"));
  CONSTEXPR_EXPECT_FALSE(alef::compatible_ignoring_case(u8"①", u8"2"));
}

CONSTEXPR_TEST(Caseless, WithALanguage) {
  // Capital I folds to dotless i in Turkish and Azerbaijani, and only there.
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u8"I", u8"\u0131", alef::casing_language::turkic));
  CONSTEXPR_EXPECT_FALSE(alef::equivalent_ignoring_case(u8"I", u8"\u0131"));
  // Dotted capital I is decomposed first (D145), to I and a combining dot,
  // so under Turkic folding it is dotless i with the dot, not i.
  CONSTEXPR_EXPECT_TRUE(alef::equivalent_ignoring_case(u8"\u0130", u8"\u0131\u0307", alef::casing_language::turkic));
}

CONSTEXPR_TEST(Caseless, KeysAreViews) {
  // The keys themselves, for hashing or looking up.
  CONSTEXPR_EXPECT_TRUE((u8"É" | alef::as_canonical_caseless | std::ranges::to<std::u32string>()) ==
                        U"é");
  CONSTEXPR_EXPECT_TRUE((u8"①X" | alef::as_compatibility_caseless | std::ranges::to<std::u32string>()) ==
                        U"1x");
}
