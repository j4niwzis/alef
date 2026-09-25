// Grapheme clusters: what a reader takes for one character.
//
// The rules are those of UAX #29, Unicode Text Segmentation, for extended
// grapheme clusters, and the properties they are decided by are read from
// the Unicode Character Database of the same version -- Grapheme_Cluster_Break
// from GraphemeBreakProperty.txt, Indic_Conjunct_Break from
// DerivedCoreProperties.txt, Extended_Pictographic from emoji-data.txt.
export module alef:grapheme;

import std;
import :utf8;
import :ucd;

export namespace alef {

// The Grapheme_Cluster_Break property.
enum class grapheme_break_property : std::uint8_t {
  other,
  cr,
  lf,
  control,
  extend,
  zwj,
  regional_indicator,
  prepend,
  spacing_mark,
  l,
  v,
  t,
  lv,
  lvt,
};

// The Indic_Conjunct_Break property.
enum class conjunct_break_property : std::uint8_t {
  none,
  linker,
  consonant,
  extend,
};

}  // namespace alef

namespace alef::tables {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char grapheme_break_bytes[] = {
#embed "../ucd/auxiliary/GraphemeBreakProperty.txt"
};
constexpr char derived_core_bytes[] = {
#embed "../ucd/DerivedCoreProperties.txt"
};
constexpr char emoji_bytes[] = {
#embed "../ucd/emoji/emoji-data.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file grapheme_break_file{grapheme_break_bytes,
                                        sizeof grapheme_break_bytes};
constexpr ucd::file derived_core_file{derived_core_bytes,
                                      sizeof derived_core_bytes};
constexpr ucd::file emoji_file{emoji_bytes, sizeof emoji_bytes};

constexpr std::optional<grapheme_break_property> grapheme_break_of(
    const ucd::line& one) {
  using enum grapheme_break_property;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, grapheme_break_property> names[] = {
      {"CR", cr},
      {"LF", lf},
      {"Control", control},
      {"Extend", extend},
      {"ZWJ", zwj},
      {"Regional_Indicator", regional_indicator},
      {"Prepend", prepend},
      {"SpacingMark", spacing_mark},
      {"L", l},
      {"V", v},
      {"T", t},
      {"LV", lv},
      {"LVT", lvt},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

constexpr std::optional<conjunct_break_property> conjunct_break_of(
    const ucd::line& one) {
  using enum conjunct_break_property;
  if (one.count != 2 || one.fields[0] != "InCB")
    return std::nullopt;
  if (one.fields[1] == "Linker")
    return linker;
  if (one.fields[1] == "Consonant")
    return consonant;
  if (one.fields[1] == "Extend")
    return extend;
  return std::nullopt;
}

constexpr std::optional<bool> pictographic_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "Extended_Pictographic")
    return std::nullopt;
  return true;
}

// The tables themselves, as values: evaluated once, where this interface is
// compiled, and read from it by whoever imports it.
constexpr auto grapheme_break =
    ucd::property<grapheme_break_property, grapheme_break_file,
                  &grapheme_break_of>;
constexpr auto conjunct_break =
    ucd::property<conjunct_break_property, derived_core_file,
                  &conjunct_break_of>;
constexpr auto pictographic =
    ucd::property<bool, emoji_file, &pictographic_of>;

}  // namespace alef::tables

export namespace alef {

constexpr grapheme_break_property grapheme_break(char32_t code_point) noexcept {
  // ASCII as the file says it: controls, CR, LF, and nothing else but Other.
  if (code_point < 0x80) {
    if (code_point == U'\r')
      return grapheme_break_property::cr;
    if (code_point == U'\n')
      return grapheme_break_property::lf;
    if (code_point < 0x20 || code_point == 0x7F)
      return grapheme_break_property::control;
    return grapheme_break_property::other;
  }
  return ucd::lookup(tables::grapheme_break, code_point,
                     grapheme_break_property::other);
}

constexpr conjunct_break_property conjunct_break(char32_t code_point) noexcept {
  return ucd::lookup(tables::conjunct_break, code_point,
                     conjunct_break_property::none);
}

constexpr bool extended_pictographic(char32_t code_point) noexcept {
  return ucd::lookup(tables::pictographic, code_point, false);
}

// The end of the grapheme cluster that begins at `from`, which is a
// boundary: the next boundary after it, or the end of the text.
template <utf8_char Char>
constexpr std::size_t next_grapheme_boundary(std::basic_string_view<Char> text,
                                             std::size_t from) noexcept {
  using enum grapheme_break_property;
  if (from >= text.size())
    return text.size();
  const decoded first = alef::decode<Char>(text.substr(from));
  grapheme_break_property before = grapheme_break(first.code_point);
  // What the rules ask of the text before a boundary, carried along instead
  // of looked back for. Each describes the text that ends at `before`.
  //
  // Regional indicators in a row (GB12, GB13): counted from `from`, which is
  // a boundary, and so after an even number of them.
  std::size_t indicators = before == regional_indicator ? 1 : 0;
  // Extended_Pictographic Extend* (GB11), and then a ZWJ after it.
  bool pictographic = extended_pictographic(first.code_point);
  bool pictographic_zwj = false;
  // InCB=Linker InCB=Extend* (GB9c).
  bool linker = conjunct_break(first.code_point) == conjunct_break_property::linker;

  std::size_t at = from + first.length;
  while (at < text.size()) {
    const decoded next = alef::decode<Char>(text.substr(at));
    const grapheme_break_property after = grapheme_break(next.code_point);
    const conjunct_break_property conjunct = conjunct_break(next.code_point);
    const bool next_pictographic = extended_pictographic(next.code_point);
    const auto is_control = [](grapheme_break_property one) {
      return one == control || one == cr || one == lf;
    };
    bool joined = false;
    if (before == cr && after == lf)
      joined = true;  // GB3
    else if (is_control(before) || is_control(after))
      joined = false;  // GB4, GB5
    else if (before == l && (after == l || after == v || after == lv || after == lvt))
      joined = true;  // GB6
    else if ((before == lv || before == v) && (after == v || after == t))
      joined = true;  // GB7
    else if ((before == lvt || before == t) && after == t)
      joined = true;  // GB8
    else if (after == extend || after == zwj || after == spacing_mark)
      joined = true;  // GB9, GB9a
    else if (before == prepend)
      joined = true;  // GB9b
    else if (linker && conjunct == conjunct_break_property::consonant)
      joined = true;  // GB9c
    else if (pictographic_zwj && next_pictographic)
      joined = true;  // GB11
    else if (before == regional_indicator && after == regional_indicator)
      joined = indicators % 2 == 1;  // GB12, GB13
    if (!joined)
      return at;  // GB999 where nothing above joined them

    indicators = after == regional_indicator ? indicators + 1 : 0;
    pictographic_zwj = pictographic && after == zwj;
    pictographic = next_pictographic || (pictographic && after == extend);
    linker = conjunct == conjunct_break_property::linker ||
             (linker && conjunct == conjunct_break_property::extend);
    before = after;
    at += next.length;
  }
  return text.size();
}

constexpr std::size_t next_grapheme_boundary(std::string_view text,
                                             std::size_t from) noexcept {
  return alef::next_grapheme_boundary<char>(text, from);
}
constexpr std::size_t next_grapheme_boundary(std::u8string_view text,
                                             std::size_t from) noexcept {
  return alef::next_grapheme_boundary<char8_t>(text, from);
}

// The grapheme clusters of some UTF-8, each a piece of the text it was given.
template <utf8_char Char>
class grapheme_view : public std::ranges::view_interface<grapheme_view<Char>> {
 public:
  class iterator {
   public:
    using value_type = std::basic_string_view<Char>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    constexpr iterator() = default;

    constexpr std::basic_string_view<Char> operator*() const noexcept {
      return text_.substr(begin_, end_ - begin_);
    }

    constexpr iterator& operator++() noexcept {
      begin_ = end_;
      end_ = alef::next_grapheme_boundary<Char>(text_, begin_);
      return *this;
    }
    constexpr iterator operator++(int) noexcept {
      iterator was = *this;
      ++*this;
      return was;
    }

    constexpr bool operator==(const iterator& other) const noexcept {
      return begin_ == other.begin_;
    }
    constexpr bool operator==(std::default_sentinel_t) const noexcept {
      return begin_ == text_.size();
    }

    // Where the cluster begins in the text, in bytes.
    constexpr std::size_t offset() const noexcept { return begin_; }

   private:
    friend grapheme_view;

    constexpr explicit iterator(std::basic_string_view<Char> text) noexcept
        : text_(text), end_(alef::next_grapheme_boundary<Char>(text, 0)) {}

    std::basic_string_view<Char> text_;
    std::size_t begin_ = 0;
    std::size_t end_ = 0;
  };

  constexpr grapheme_view() = default;
  constexpr explicit grapheme_view(std::basic_string_view<Char> text) noexcept
      : text_(text) {}

  constexpr iterator begin() const noexcept { return iterator(text_); }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  std::basic_string_view<Char> text_;
};

constexpr grapheme_view<char> graphemes(std::string_view text) noexcept {
  return grapheme_view<char>(text);
}
constexpr grapheme_view<char8_t> graphemes(std::u8string_view text) noexcept {
  return grapheme_view<char8_t>(text);
}

}  // namespace alef

// It holds a view of the text, not the text.
template <class Char>
inline constexpr bool
    std::ranges::enable_borrowed_range<alef::grapheme_view<Char>> = true;
