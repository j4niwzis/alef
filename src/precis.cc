// SPDX-License-Identifier: AGPL-3.0-only
// Preparing strings for comparison: the PRECIS framework, RFC 8264, and its
// profiles for usernames and passwords, RFC 8265; with what it and IDNA2008
// share -- the derived property values of RFC 5892 and RFC 8264, the
// contextual rules of RFC 5892, appendix A, and the Bidi Rule of RFC 5893.
export module alef.precis;

import std;
import alef.utf;
import alef.normalization;
import alef.casing;
import alef.properties;
import alef.identifier;
import alef.bidi;
import alef.tables.general_category;
import alef.tables.preparation;

namespace alef::detail::precis {

using gc = general_category;

constexpr bool noncharacter(char32_t cp) noexcept {
  return (cp >= 0xFDD0 && cp <= 0xFDEF) || (cp & 0xFFFE) == 0xFFFE;
}

// White_Space (PropList.txt), which has not changed since Unicode 6.1.
constexpr bool white_space(char32_t cp) noexcept {
  return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
         (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 || cp == 0x202F ||
         cp == 0x205F || cp == 0x3000;
}

constexpr bool letter_digit(gc one) noexcept {
  return one == gc::lowercase_letter || one == gc::uppercase_letter || one == gc::other_letter ||
         one == gc::decimal_number || one == gc::modifier_letter || one == gc::nonspacing_mark ||
         one == gc::spacing_mark;
}

constexpr bool mark(gc one) noexcept {
  return one == gc::nonspacing_mark || one == gc::spacing_mark || one == gc::enclosing_mark;
}

// A code point in a string of them, as a string.
constexpr std::u32string one_of(char32_t cp) { return std::u32string(1, cp); }

constexpr std::u32string nfkc(std::u32string_view text) {
  return std::ranges::to<std::u32string>(text | as_nfkc);
}

}  // namespace alef::detail::precis

export namespace alef {

// The derived property values of RFC 8264 and RFC 5892.
enum class derived_property : std::uint8_t {
  pvalid,      // PVALID: allowed
  contextj,    // CONTEXTJ: a joiner, allowed where its rule holds
  contexto,    // CONTEXTO: allowed where its rule holds
  disallowed,  // DISALLOWED
  free_pval,   // ID_DIS or FREE_PVAL: allowed in free-form strings, not identifiers
  unassigned,  // UNASSIGNED
};

namespace detail::precis {

// The exceptions of RFC 5892, section 2.6, which both take first.
constexpr std::optional<derived_property> exception(char32_t cp) noexcept {
  using enum derived_property;
  switch (cp) {
    case 0x00DF: case 0x03C2: case 0x06FD: case 0x06FE: case 0x0F0B: case 0x3007:
      return pvalid;
    case 0x00B7: case 0x0375: case 0x05F3: case 0x05F4: case 0x30FB:
      return contexto;
    case 0x0640: case 0x07FA: case 0x302E: case 0x302F: case 0x303B:
      return disallowed;
    default:
      break;
  }
  if ((cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9))
    return contexto;
  if (cp >= 0x3031 && cp <= 0x3035)
    return disallowed;
  return std::nullopt;
}

constexpr bool unassigned(char32_t cp) noexcept {
  return general_category_of(cp) == gc::unassigned && !noncharacter(cp);
}

}  // namespace detail::precis

// A code point's derived property value in PRECIS (RFC 8264, section 8).
constexpr derived_property precis_property_of(char32_t cp) {
  using namespace detail::precis;
  using enum derived_property;
  if (const auto value = exception(cp))
    return *value;
  if (detail::precis::unassigned(cp))
    return derived_property::unassigned;
  if (cp >= 0x21 && cp <= 0x7E)
    return pvalid;  // ASCII7
  if (cp == 0x200C || cp == 0x200D)
    return contextj;  // JoinControl
  if (tables::conjoining_jamo(cp))
    return disallowed;  // OldHangulJamo
  if (is_default_ignorable(cp) || noncharacter(cp))
    return disallowed;  // PrecisIgnorableProperties
  const gc category = general_category_of(cp);
  if (category == gc::control)
    return disallowed;  // Controls
  if (nfkc(one_of(cp)) != one_of(cp))
    return free_pval;  // HasCompat
  if (letter_digit(category))
    return pvalid;  // LetterDigits
  switch (category) {
    case gc::titlecase_letter: case gc::letter_number: case gc::other_number:
    case gc::enclosing_mark:  // OtherLetterDigits
    case gc::space_separator:  // Spaces
    case gc::math_symbol: case gc::currency_symbol: case gc::modifier_symbol:
    case gc::other_symbol:  // Symbols
    case gc::connector_punctuation: case gc::dash_punctuation: case gc::open_punctuation:
    case gc::close_punctuation: case gc::initial_punctuation: case gc::final_punctuation:
    case gc::other_punctuation:  // Punctuation
      return free_pval;
    default:
      return disallowed;
  }
}

// A code point's derived property value in IDNA2008 (RFC 5892, section 3).
constexpr derived_property idna_property_of(char32_t cp) {
  using namespace detail::precis;
  using enum derived_property;
  if (const auto value = exception(cp))
    return *value;
  if (detail::precis::unassigned(cp))
    return derived_property::unassigned;
  if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') || cp == '-')
    return pvalid;  // LDH
  if (cp == 0x200C || cp == 0x200D)
    return contextj;  // JoinControl
  const std::u32string itself = one_of(cp);
  if ((std::ranges::to<std::u32string>(nfkc(itself) | as_folded | as_nfkc)) != itself)
    return disallowed;  // Unstable
  if (is_default_ignorable(cp) || white_space(cp) || noncharacter(cp))
    return disallowed;  // IgnorableProperties
  if ((cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0x1D100 && cp <= 0x1D24F))
    return disallowed;  // IgnorableBlocks
  if (tables::conjoining_jamo(cp))
    return disallowed;  // OldHangulJamo
  return letter_digit(general_category_of(cp)) ? pvalid : disallowed;
}

// Whether the rule for the CONTEXTJ or CONTEXTO code point at `at` holds in
// `label` (RFC 5892, appendix A). A code point with no rule never holds.
constexpr bool context_rule_holds(std::u32string_view label, std::size_t at) {
  const char32_t cp = label[at];
  const auto before = [&](std::size_t n) { return at >= n ? label[at - n] : char32_t{0}; };
  const bool has_before = at > 0;
  const bool has_after = at + 1 < label.size();
  constexpr std::uint8_t virama = 9;
  switch (cp) {
    case 0x200C: {  // ZERO WIDTH NON-JOINER
      if (has_before && canonical_combining_class(before(1)) == virama)
        return true;
      std::size_t left = at;
      while (left > 0 && tables::joining_type_of(label[left - 1]) == joining_type::transparent)
        --left;
      if (left == 0)
        return false;
      const joining_type l = tables::joining_type_of(label[left - 1]);
      if (l != joining_type::left_joining && l != joining_type::dual_joining)
        return false;
      std::size_t right = at + 1;
      while (right < label.size() && tables::joining_type_of(label[right]) == joining_type::transparent)
        ++right;
      if (right == label.size())
        return false;
      const joining_type r = tables::joining_type_of(label[right]);
      return r == joining_type::right_joining || r == joining_type::dual_joining;
    }
    case 0x200D:  // ZERO WIDTH JOINER
      return has_before && canonical_combining_class(before(1)) == virama;
    case 0x00B7:  // MIDDLE DOT, as in Catalan l·l
      return has_before && has_after && before(1) == U'l' && label[at + 1] == U'l';
    case 0x0375:  // GREEK LOWER NUMERAL SIGN (KERAIA)
      return has_after && script_of(label[at + 1]) == script("Greek");
    case 0x05F3:  // HEBREW PUNCTUATION GERESH
    case 0x05F4:  // HEBREW PUNCTUATION GERSHAYIM
      return has_before && script_of(before(1)) == script("Hebrew");
    case 0x30FB:  // KATAKANA MIDDLE DOT
      return std::ranges::any_of(label, [](char32_t one) {
        const script s = script_of(one);
        return s == script("Hiragana") || s == script("Katakana") || s == script("Han");
      });
    default:
      break;
  }
  if (cp >= 0x0660 && cp <= 0x0669)  // ARABIC-INDIC DIGITS
    return std::ranges::none_of(label, [](char32_t one) { return one >= 0x06F0 && one <= 0x06F9; });
  if (cp >= 0x06F0 && cp <= 0x06F9)  // EXTENDED ARABIC-INDIC DIGITS
    return std::ranges::none_of(label, [](char32_t one) { return one >= 0x0660 && one <= 0x0669; });
  return false;
}

// Whether a string has a right-to-left code point: Bidi_Class R, AL or AN.
constexpr bool has_right_to_left(std::u32string_view text) {
  return std::ranges::any_of(text, [](char32_t one) {
    const bidi_class c = bidi_class_of(one);
    return c == bidi_class::r || c == bidi_class::al || c == bidi_class::an;
  });
}

// Whether a string satisfies the Bidi Rule of RFC 5893, section 2.
constexpr bool satisfies_bidi_rule(std::u32string_view text) {
  using bc = bidi_class;
  if (text.empty())
    return true;
  const bc first = bidi_class_of(text.front());
  if (first != bc::l && first != bc::r && first != bc::al)
    return false;  // 1
  const bool rtl = first != bc::l;
  bool en = false, an = false;
  for (const char32_t one : text) {
    const bc c = bidi_class_of(one);
    const bool allowed =
        rtl ? (c == bc::r || c == bc::al || c == bc::an || c == bc::en || c == bc::es ||
               c == bc::cs || c == bc::et || c == bc::on || c == bc::bn || c == bc::nsm)  // 2
            : (c == bc::l || c == bc::en || c == bc::es || c == bc::cs || c == bc::et ||
               c == bc::on || c == bc::bn || c == bc::nsm);  // 5
    if (!allowed)
      return false;
    en = en || c == bc::en;
    an = an || c == bc::an;
  }
  if (rtl && en && an)
    return false;  // 4
  std::size_t end = text.size();
  while (end > 0 && bidi_class_of(text[end - 1]) == bc::nsm)
    --end;
  if (end == 0)
    return false;
  const bc last = bidi_class_of(text[end - 1]);
  return rtl ? (last == bc::r || last == bc::al || last == bc::en || last == bc::an)  // 3
             : (last == bc::l || last == bc::en);                                       // 6
}

// Fullwidth and halfwidth code points mapped to their decompositions: the
// Width Mapping Rule of RFC 8264, section 9.2.
constexpr std::u32string width_mapped(std::u32string_view text) {
  std::u32string out;
  for (const char32_t one : text) {
    if (tables::wide_or_narrow(one))
      out += detail::precis::nfkc(std::u32string_view(&one, 1));
    else
      out.push_back(one);
  }
  return out;
}

namespace detail::precis {

// Whether every code point of a prepared string is allowed by a string class.
constexpr bool allowed(std::u32string_view text, bool freeform) {
  for (std::size_t at = 0; at < text.size(); ++at) {
    switch (precis_property_of(text[at])) {
      case derived_property::pvalid:
        break;
      case derived_property::free_pval:
        if (!freeform)
          return false;
        break;
      case derived_property::contextj:
      case derived_property::contexto:
        if (!context_rule_holds(text, at))
          return false;
        break;
      default:
        return false;
    }
  }
  return true;
}

}  // namespace detail::precis

// A username prepared and enforced by the UsernameCaseMapped profile of RFC
// 8265, section 3.3 -- width mapping, lower case, NFC, the Bidi Rule where
// it has right-to-left code points, the IdentifierClass -- or nothing where
// it is not one. With `case_mapped` false, the UsernameCasePreserved profile
// (section 3.4), which keeps case.
template <std::ranges::viewable_range Range>
  requires utf_range<Range>
constexpr std::optional<std::u32string> prepare_username(Range&& text, bool case_mapped = true) {
  std::u32string out = width_mapped(std::ranges::to<std::u32string>(std::forward<Range>(text) | as_utf32));
  if (case_mapped)
    out = std::ranges::to<std::u32string>(out | as_lower);
  out = std::ranges::to<std::u32string>(out | as_nfc);
  if (out.empty() || (has_right_to_left(out) && !satisfies_bidi_rule(out)) ||
      !detail::precis::allowed(out, false))
    return std::nullopt;
  return out;
}

// A string prepared and enforced by the OpaqueString profile of RFC 8265,
// section 4.2 -- as for passwords: spaces other than U+0020 mapped to it,
// NFC, the FreeformClass -- or nothing where it is not one.
template <std::ranges::viewable_range Range>
  requires utf_range<Range>
constexpr std::optional<std::u32string> prepare_opaque_string(Range&& text) {
  std::u32string out;
  for (const char32_t one : std::forward<Range>(text) | as_utf32)
    out.push_back(general_category_of(one) == general_category::space_separator ? U' ' : one);
  out = std::ranges::to<std::u32string>(out | as_nfc);
  if (out.empty() || !detail::precis::allowed(out, true))
    return std::nullopt;
  return out;
}

}  // namespace alef
