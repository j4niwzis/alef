// Caseless matching, D144 to D146 of the Unicode Standard, by example: what
// each kind of match lets through and what it does not.
import std;
import alef.utf;
import alef.casing;
import alef.caseless;
import alef.normalization;
import alef.identifier;
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

namespace {

// NFKC_Casefold of one code point by its definition (DerivedNormalizationProps.txt):
// NFKC, case folding and removing Default_Ignorable_Code_Point, over and
// over until nothing changes.
std::u32string by_definition(char32_t code_point) {
  std::u32string text(1, code_point);
  for (;;) {
    std::u32string next;
    for (const char32_t one : text | alef::as_nfkc | alef::as_folded)
      if (!alef::is_default_ignorable(one))
        next.push_back(one);
    if (next == text)  // toNFKC_Casefold then puts the whole string in NFC
      return next | alef::as_nfc | std::ranges::to<std::u32string>();
    text = next;
  }
}

}  // namespace

// Every code point, against its definition. Only when run: while compiled,
// by example below.
TEST(Caseless, NfkcCasefoldOfEveryCodePoint) {
  std::string wrong;
  for (char32_t code_point = 0; code_point <= 0x10FFFF; ++code_point) {
    if (code_point >= 0xD800 && code_point <= 0xDFFF)
      continue;
    const std::u32string ours = std::u32string(1, code_point) | alef::as_nfkc_casefold |
                                std::ranges::to<std::u32string>();
    if (ours != by_definition(code_point) && wrong.size() < 400) {
      char hex[16];
      std::snprintf(hex, sizeof hex, "%04X ", static_cast<unsigned>(code_point));
      wrong += hex;
    }
  }
  EXPECT_EQ(wrong, "");
}

CONSTEXPR_TEST(Caseless, NfkcCasefoldByExample) {
  const auto folded = [](std::u8string_view text) {
    return text | alef::as_nfkc_casefold | std::ranges::to<std::u32string>();
  };
  CONSTEXPR_EXPECT_TRUE(folded(u8"Stra\u00DFe") == U"strasse");
  CONSTEXPR_EXPECT_TRUE(folded(u8"\u212B") == U"\u00E5");     // ANGSTROM SIGN
  CONSTEXPR_EXPECT_TRUE(folded(u8"\uFB01") == U"fi");          // a ligature
  CONSTEXPR_EXPECT_TRUE(folded(u8"\u2460") == U"1");           // circled
  CONSTEXPR_EXPECT_TRUE(folded(u8"a\u00ADb") == U"ab");        // a soft hyphen goes
  CONSTEXPR_EXPECT_TRUE(folded(u8"\uFF21\uFF22") == U"ab");   // full-width
  CONSTEXPR_EXPECT_TRUE(folded(u8"e\u0301") == U"\u00E9");    // and then NFC
  CONSTEXPR_EXPECT_TRUE(alef::equal_by_nfkc_casefold(u8"\uFF26\u0131le", u8"FILE") == false);
  CONSTEXPR_EXPECT_TRUE(alef::equal_by_nfkc_casefold(u8"\uFF26ILE", u"file"));
}
