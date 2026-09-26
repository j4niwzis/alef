// SPDX-License-Identifier: AGPL-3.0-only
// Internationalized domain names: IDNA2008 -- labels by the rules of RFC
// 5891 and RFC 5892, the Bidi Rule of RFC 5893 across a domain name, and
// Punycode between U-labels and A-labels -- with the mapping of RFC 5895
// (lower case, fullwidth and halfwidth code points, NFC) before them.
export module alef.idna;

import std;
import alef.utf;
import alef.normalization;
import alef.casing;
import alef.properties;
import alef.punycode;
import alef.precis;
import alef.tables.general_category;

namespace alef::detail::idna {

constexpr bool separator(char32_t one) noexcept {
  return one == U'.' || one == 0x3002 || one == 0xFF0E || one == 0xFF61;
}

constexpr bool ascii(std::u32string_view text) noexcept {
  return std::ranges::all_of(text, [](char32_t one) { return one < 0x80; });
}

constexpr bool hyphens_fit(std::u32string_view label) noexcept {
  if (label.front() == U'-' || label.back() == U'-')
    return false;
  return !(label.size() >= 4 && label[2] == U'-' && label[3] == U'-');
}

// Whether a string is a valid U-label (RFC 5891, section 5.4; RFC 5892).
constexpr bool u_label(std::u32string_view label) {
  if (label.empty() || !hyphens_fit(label))
    return false;
  if ((label | as_nfc | std::ranges::to<std::u32string>()) != label)
    return false;
  const general_category first = general_category_of(label.front());
  if (first == general_category::nonspacing_mark || first == general_category::spacing_mark ||
      first == general_category::enclosing_mark)
    return false;
  for (std::size_t at = 0; at < label.size(); ++at) {
    switch (idna_property_of(label[at])) {
      case derived_property::pvalid:
        break;
      case derived_property::contextj:
      case derived_property::contexto:
        if (!context_rule_holds(label, at))
          return false;
        break;
      default:
        return false;
    }
  }
  return true;
}

// Whether an ASCII label is a valid LDH label that is not an A-label.
constexpr bool ldh_label(std::u32string_view label) {
  if (label.empty() || !hyphens_fit(label))
    return false;
  return std::ranges::all_of(label, [](char32_t one) {
    return (one >= U'a' && one <= U'z') || (one >= U'0' && one <= U'9') || one == U'-';
  });
}

// A domain name's labels as U-labels, and whether it ends in a root dot; or
// nothing where a label is not valid, or the Bidi Rule fails.
struct labels {
  std::vector<std::u32string> unicode;
  bool rooted = false;
};

// A label as its U-label, or nothing where it is not valid: an A-label is
// decoded, and must be what encoding its U-label gives.
constexpr std::optional<std::u32string> unicode_label(const std::u32string& label) {
  if (ascii(label) && label.starts_with(U"xn--")) {
    std::string encoded;
    for (const char32_t one : std::u32string_view(label).substr(4))
      encoded.push_back(static_cast<char>(one));
    const auto decoded = punycode_decode(encoded);
    if (!decoded || decoded->empty() || ascii(*decoded) || !u_label(*decoded))
      return std::nullopt;
    const auto again = punycode_encode(*decoded);
    if (!again || *again != encoded)
      return std::nullopt;
    return *decoded;
  }
  if (ascii(label) ? ldh_label(label) : u_label(label))
    return label;
  return std::nullopt;
}

template <class Range>
constexpr std::optional<labels> read(Range&& text) {
  // RFC 5895: lower case, the width mapping, NFC.
  const std::u32string mapped =
      width_mapped(std::forward<Range>(text) | as_utf32 | as_lower | std::ranges::to<std::u32string>()) |
      as_nfc | std::ranges::to<std::u32string>();
  std::vector<std::u32string> parts(1);
  for (const char32_t one : mapped) {
    if (separator(one))
      parts.emplace_back();
    else
      parts.back().push_back(one);
  }
  labels out;
  if (parts.size() > 1 && parts.back().empty()) {
    out.rooted = true;
    parts.pop_back();
  }
  for (const std::u32string& part : parts) {
    const auto label = unicode_label(part);
    if (!label)
      return std::nullopt;
    out.unicode.push_back(*label);
  }
  // A domain name with a right-to-left label: every label keeps the Bidi Rule.
  if (std::ranges::any_of(out.unicode, [](const std::u32string& one) { return has_right_to_left(one); }) &&
      !std::ranges::all_of(out.unicode, [](const std::u32string& one) { return satisfies_bidi_rule(one); }))
    return std::nullopt;
  return out;
}

}  // namespace alef::detail::idna

export namespace alef {

// A domain name in any UTF as ASCII, its labels A-labels where they are not
// LDH -- "bücher.de" as "xn--bcher-kva.de" -- or nothing where it is not a
// valid IDNA2008 domain name: a label not valid, longer than 63 bytes, the
// whole longer than 253, or the Bidi Rule broken.
template <std::ranges::viewable_range Range>
  requires utf_range<Range>
constexpr std::optional<std::string> idna_to_ascii(Range&& text) {
  const auto labels = detail::idna::read(std::forward<Range>(text));
  if (!labels)
    return std::nullopt;
  std::string out;
  for (const std::u32string& label : labels->unicode) {
    std::string one;
    if (detail::idna::ascii(label)) {
      for (const char32_t unit : label)
        one.push_back(static_cast<char>(unit));
    } else {
      const auto encoded = punycode_encode(label);
      if (!encoded)
        return std::nullopt;
      one = "xn--" + *encoded;
    }
    if (one.size() > 63)
      return std::nullopt;
    if (!out.empty())
      out.push_back('.');
    out += one;
  }
  if (out.size() > 253)
    return std::nullopt;
  if (labels->rooted)
    out.push_back('.');
  return out;
}

// A domain name in any UTF with its A-labels as U-labels -- "xn--bcher-kva.de"
// as "bücher.de" -- or nothing where it is not a valid IDNA2008 domain name.
template <std::ranges::viewable_range Range>
  requires utf_range<Range>
constexpr std::optional<std::u32string> idna_to_unicode(Range&& text) {
  const auto labels = detail::idna::read(std::forward<Range>(text));
  if (!labels)
    return std::nullopt;
  std::u32string out;
  for (const std::u32string& label : labels->unicode) {
    if (!out.empty())
      out.push_back(U'.');
    out += label;
  }
  if (labels->rooted)
    out.push_back(U'.');
  return out;
}

}  // namespace alef
