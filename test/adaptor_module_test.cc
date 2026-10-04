// SPDX-License-Identifier: AGPL-3.0-only
// A separate importer: compiling the adaptor's own module does not expose
// a missing pipe operator in the consumer's argument-dependent lookup.
import std;
import alef.utf;
import alef.grapheme;
import alef.line;
import alef.bidi;

constexpr bool utf_pipes() {
  const std::string_view text = "A\xc3\xa9";
  const auto decoded = text | alef::as_utf32;
  if (!std::ranges::equal(decoded, std::u32string_view(U"Aé")))
    return false;
  if (!std::ranges::equal(text | alef::as_utf32 | alef::as_utf8,
                          std::u8string_view(u8"Aé")))
    return false;
  // Lvalue views and owned rvalue strings must retain their usual lifetimes.
  auto view = text;
  if (!std::ranges::equal(std::ranges::ref_view(view) | alef::as_utf32, decoded))
    return false;
  const auto owned = std::string("A\xc3\xa9") | alef::as_utf32;
  return std::ranges::equal(owned, decoded);
}
static_assert(utf_pipes());

int main() {
  if (!utf_pipes())
    return 1;
  const std::string_view text = "a\xcc\x81 b";
  if (std::ranges::distance(text | alef::graphemes) != 3)
    return 2;
  if (std::ranges::distance(text | alef::graphemes(alef::owning<>)) != 3)
    return 3;
  if (std::ranges::distance(std::string_view("one two") | alef::line_breaks) != 2)
    return 4;
  // This constructor also applies as_utf32 inside an imported template.
  const alef::bidi_paragraph paragraph(text);
  if (paragraph.size() != 4 || paragraph.level() != 0)
    return 5;
}
