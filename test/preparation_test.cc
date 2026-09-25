// String preparation: Punycode against the sample strings of RFC 3492; the
// derived property values, context rules and Bidi Rule of RFC 5892, 5893 and
// 8264; the username and password profiles of RFC 8265 against its own
// examples; and IDNA2008 domain names.
import std;
import alef.utf;
import alef.punycode;
import alef.precis;
import alef.idna;
import alef.tables.preparation;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "punycode_vectors.h"

namespace {

constexpr std::string lower(std::string_view text) {
  std::string out(text);
  for (char& one : out)
    if (one >= 'A' && one <= 'Z')
      one = static_cast<char>(one - 'A' + 'a');
  return out;
}

constexpr std::optional<std::u32string> username(std::u32string_view text) {
  return alef::prepare_username(text);
}

}  // namespace

CONSTEXPR_TEST(Punycode, SampleStringsOfRfc3492) {
  // The RFC marks some letters of its encodings upper case, to carry the case
  // of the letters they stand for; alef writes them lower case.
  std::size_t wrong = 0;
  for (const punycode_vector& one : punycode_vectors) {
    const auto encoded = alef::punycode_encode(one.text);
    const auto decoded = alef::punycode_decode(one.encoded);
    wrong += !encoded || lower(*encoded) != lower(one.encoded) || !decoded || *decoded != one.text;
  }
  CONSTEXPR_EXPECT_EQ(std::size(punycode_vectors), std::size_t{19});
  CONSTEXPR_EXPECT_EQ(wrong, std::size_t{0});
}

CONSTEXPR_TEST(Punycode, WhatIsNotPunycode) {
  CONSTEXPR_EXPECT_TRUE(alef::punycode_encode(U"bücher") == std::optional<std::string>("bcher-kva"));
  CONSTEXPR_EXPECT_FALSE(alef::punycode_decode("bcher-kv!").has_value());  // not a digit
  CONSTEXPR_EXPECT_FALSE(alef::punycode_decode("99999999999").has_value());  // overflow
  CONSTEXPR_EXPECT_FALSE(alef::punycode_decode("\xC3\xBC-kva").has_value());  // not ASCII
}

CONSTEXPR_TEST(Preparation, TheirTables) {
  using alef::joining_type;
  CONSTEXPR_EXPECT_TRUE(alef::tables::joining_type_of(U'ب') == joining_type::dual_joining);
  CONSTEXPR_EXPECT_TRUE(alef::tables::joining_type_of(U'ا') == joining_type::right_joining);
  CONSTEXPR_EXPECT_TRUE(alef::tables::joining_type_of(U'ً') == joining_type::transparent);
  CONSTEXPR_EXPECT_TRUE(alef::tables::joining_type_of(U'a') == joining_type::non_joining);
  CONSTEXPR_EXPECT_TRUE(alef::tables::conjoining_jamo(U'ᄀ'));
  CONSTEXPR_EXPECT_FALSE(alef::tables::conjoining_jamo(U'가'));  // a syllable
  CONSTEXPR_EXPECT_TRUE(alef::tables::wide_or_narrow(U'Ａ'));
  CONSTEXPR_EXPECT_TRUE(alef::tables::wide_or_narrow(U'｡'));
  CONSTEXPR_EXPECT_FALSE(alef::tables::wide_or_narrow(U'A'));
}

CONSTEXPR_TEST(Precis, DerivedProperties) {
  using enum alef::derived_property;
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'a') == pvalid);
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'@') == pvalid);        // ASCII7
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U' ') == free_pval);      // a space
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'ß') == pvalid);   // an exception
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'ـ') == disallowed);
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'‍') == contextj);
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'·') == contexto);
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'Ａ') == free_pval);  // HasCompat
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'ᄀ') == disallowed);  // old jamo
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'͸') == unassigned);
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'﷐') == disallowed);  // noncharacter
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'­') == disallowed);  // ignorable
  CONSTEXPR_EXPECT_TRUE(alef::precis_property_of(U'∞') == free_pval);   // a symbol
  CONSTEXPR_EXPECT_TRUE(alef::idna_property_of(U'A') == disallowed);          // unstable
  CONSTEXPR_EXPECT_TRUE(alef::idna_property_of(U'@') == disallowed);
  CONSTEXPR_EXPECT_TRUE(alef::idna_property_of(U'ü') == pvalid);
}

CONSTEXPR_TEST(Precis, ContextRules) {
  CONSTEXPR_EXPECT_TRUE(username(U"l·l").has_value());               // Catalan
  CONSTEXPR_EXPECT_FALSE(username(U"a·b").has_value());
  CONSTEXPR_EXPECT_TRUE(username(U"क्‌ष").has_value());  // after a virama
  CONSTEXPR_EXPECT_FALSE(username(U"a‌b").has_value());
  CONSTEXPR_EXPECT_TRUE(username(U"ب‌ب").has_value());     // between joining letters
  CONSTEXPR_EXPECT_TRUE(username(U"͵α").has_value());           // Greek keraia
  CONSTEXPR_EXPECT_FALSE(username(U"͵a").has_value());
  CONSTEXPR_EXPECT_TRUE(username(U"א׳").has_value());           // Hebrew geresh
  CONSTEXPR_EXPECT_TRUE(username(U"カ・カ").has_value());     // katakana middle dot
  CONSTEXPR_EXPECT_FALSE(username(U"a・b").has_value());
  CONSTEXPR_EXPECT_FALSE(username(U"٠۰").has_value());          // both kinds of digits
}

CONSTEXPR_TEST(Precis, BidiRule) {
  CONSTEXPR_EXPECT_TRUE(alef::satisfies_bidi_rule(U"שלום"));
  CONSTEXPR_EXPECT_TRUE(alef::satisfies_bidi_rule(U"abc1"));
  CONSTEXPR_EXPECT_FALSE(alef::satisfies_bidi_rule(U"1abc"));               // rule 1
  CONSTEXPR_EXPECT_FALSE(alef::satisfies_bidi_rule(U"aא"));            // rule 5
  CONSTEXPR_EXPECT_FALSE(alef::satisfies_bidi_rule(U"אa"));            // rule 2
  CONSTEXPR_EXPECT_FALSE(alef::satisfies_bidi_rule(U"ا١1"));  // rule 4: AN and EN
  CONSTEXPR_EXPECT_TRUE(alef::satisfies_bidi_rule(U"א́"));        // rule 3, with NSM
}

// RFC 8265, section 3.5: examples of usernames.
CONSTEXPR_TEST(Precis, UsernameExamplesOfRfc8265) {
  CONSTEXPR_EXPECT_TRUE(username(U"juliet@example.com") == std::optional<std::u32string>(U"juliet@example.com"));
  CONSTEXPR_EXPECT_TRUE(username(U"fussball") == std::optional<std::u32string>(U"fussball"));
  CONSTEXPR_EXPECT_TRUE(username(U"fußball") == std::optional<std::u32string>(U"fußball"));
  CONSTEXPR_EXPECT_TRUE(username(U"π") == std::optional<std::u32string>(U"π"));
  CONSTEXPR_EXPECT_TRUE(username(U"Σ") == std::optional<std::u32string>(U"σ"));
  CONSTEXPR_EXPECT_TRUE(username(U"ς") == std::optional<std::u32string>(U"ς"));
  CONSTEXPR_EXPECT_FALSE(username(U"foo bar").has_value());      // a space
  CONSTEXPR_EXPECT_FALSE(username(U"").has_value());             // empty
  CONSTEXPR_EXPECT_FALSE(username(U"henryⅣ").has_value());  // a compatibility character
  CONSTEXPR_EXPECT_FALSE(username(U"♚").has_value());       // a symbol
  // Fullwidth letters are mapped to theirs, and case to lower.
  CONSTEXPR_EXPECT_TRUE(username(U"Ｊｕｌｉｅｔ") == std::optional<std::u32string>(U"juliet"));
  // Case kept, by the other profile.
  CONSTEXPR_EXPECT_TRUE(alef::prepare_username(U"Juliet", false) == std::optional<std::u32string>(U"Juliet"));
  // In any UTF.
  CONSTEXPR_EXPECT_TRUE(alef::prepare_username(u8"Juliet") == std::optional<std::u32string>(U"juliet"));
}

// RFC 8265, section 4.3: examples of passwords.
CONSTEXPR_TEST(Precis, OpaqueStringExamplesOfRfc8265) {
  CONSTEXPR_EXPECT_TRUE(alef::prepare_opaque_string(U"correct horse battery staple").has_value());
  CONSTEXPR_EXPECT_TRUE(alef::prepare_opaque_string(U"Correct Horse Battery Staple") ==
                        std::optional<std::u32string>(U"Correct Horse Battery Staple"));
  CONSTEXPR_EXPECT_TRUE(alef::prepare_opaque_string(U"πßå").has_value());
  CONSTEXPR_EXPECT_TRUE(alef::prepare_opaque_string(U"Jack of ♦s").has_value());
  CONSTEXPR_EXPECT_TRUE(alef::prepare_opaque_string(U"foo bar") == std::optional<std::u32string>(U"foo bar"));
  CONSTEXPR_EXPECT_FALSE(alef::prepare_opaque_string(U"").has_value());
  CONSTEXPR_EXPECT_FALSE(alef::prepare_opaque_string(U"my cat is a \u0009by").has_value());
}

CONSTEXPR_TEST(Idna, DomainNames) {
  using ascii = std::optional<std::string>;
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"bücher.de") == ascii("xn--bcher-kva.de"));
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"München.DE") == ascii("xn--mnchen-3ya.de"));
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"例え.テスト") == ascii("xn--r8jz45g.xn--zckzah"));
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"EXAMPLE.com.") == ascii("example.com."));
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"bücher。de") == ascii("xn--bcher-kva.de"));  // ideographic stop
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_unicode(u8"xn--bcher-kva.de") == std::optional<std::u32string>(U"bücher.de"));
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"-abc.com").has_value());      // a hyphen first
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"ab--c.com").has_value());     // hyphens 3 and 4
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"ex ample.com").has_value());  // a space
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"a..b").has_value());          // an empty label
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"́a.com").has_value());   // a mark first
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_unicode(u8"xn--bcher-kv.de").has_value());  // not what it decodes to
  // A right-to-left label makes every label keep the Bidi Rule.
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(u8"1com.org").has_value());
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(u8"שלום.1com").has_value());
  const auto hebrew = alef::idna_to_ascii(u8"שלום.com");
  CONSTEXPR_EXPECT_TRUE(hebrew.has_value() && alef::idna_to_unicode(*hebrew) ==
                                                   std::optional<std::u32string>(U"שלום.com"));
  // A label of 63 bytes, and one of 64.
  CONSTEXPR_EXPECT_TRUE(alef::idna_to_ascii(std::string(63, 'a') + ".com").has_value());
  CONSTEXPR_EXPECT_FALSE(alef::idna_to_ascii(std::string(64, 'a') + ".com").has_value());
}
