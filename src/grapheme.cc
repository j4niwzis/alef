// Grapheme clusters: what a reader takes for one character.
//
// The rules are those of UAX #29, Unicode Text Segmentation, revision 49, for
// extended grapheme clusters, and the properties they are decided by are read
// from the Unicode Character Database of the same version:
// Grapheme_Cluster_Break from GraphemeBreakProperty.txt, Indic_Conjunct_Break
// from DerivedCoreProperties.txt, Extended_Pictographic from emoji-data.txt.
export module alef:grapheme;

import std;
import :utf8;
import :ucd;

export namespace alef {

// The Grapheme_Cluster_Break property, and a code point's value of it.
enum class grapheme_cluster_break : std::uint8_t {
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
enum class indic_conjunct_break : std::uint8_t {
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

constexpr std::optional<grapheme_cluster_break> grapheme_break_of(
    const ucd::line& one) {
  using enum grapheme_cluster_break;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, grapheme_cluster_break> names[] = {
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

constexpr std::optional<indic_conjunct_break> conjunct_break_of(
    const ucd::line& one) {
  using enum indic_conjunct_break;
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
    ucd::property<grapheme_cluster_break, grapheme_break_file,
                  &grapheme_break_of>;
constexpr auto conjunct_break =
    ucd::property<indic_conjunct_break, derived_core_file,
                  &conjunct_break_of>;
constexpr auto pictographic =
    ucd::property<bool, emoji_file, &pictographic_of>;

}  // namespace alef::tables

export namespace alef {

constexpr grapheme_cluster_break grapheme_cluster_break_of(
    char32_t code_point) noexcept {
  // ASCII as the file says it: controls, CR, LF, and nothing else but Other.
  if (code_point < 0x80) {
    if (code_point == U'\r')
      return grapheme_cluster_break::cr;
    if (code_point == U'\n')
      return grapheme_cluster_break::lf;
    if (code_point < 0x20 || code_point == 0x7F)
      return grapheme_cluster_break::control;
    return grapheme_cluster_break::other;
  }
  return ucd::lookup(tables::grapheme_break, code_point,
                     grapheme_cluster_break::other);
}

constexpr indic_conjunct_break indic_conjunct_break_of(
    char32_t code_point) noexcept {
  return ucd::lookup(tables::conjunct_break, code_point,
                     indic_conjunct_break::none);
}

constexpr bool is_extended_pictographic(char32_t code_point) noexcept {
  return ucd::lookup(tables::pictographic, code_point, false);
}

}  // namespace alef

namespace alef::detail {

constexpr bool is_control(grapheme_cluster_break one) noexcept {
  using enum grapheme_cluster_break;
  return one == control || one == cr || one == lf;
}

// The end of the cluster that begins at `from`, a boundary: the next boundary
// after it, or the end of the text. Read forwards, carrying what the rules
// ask of the text before a boundary instead of looking back for it.
template <utf8_char Char>
constexpr std::size_t next_boundary(std::basic_string_view<Char> text,
                                    std::size_t from) noexcept {
  using enum grapheme_cluster_break;
  if (from >= text.size())
    return text.size();
  const decoded first = decode_front(text.substr(from));
  grapheme_cluster_break before = grapheme_cluster_break_of(first.code_point);
  // Each describes the text that ends at `before`.
  //
  // Regional indicators in a row (GB12, GB13): counted from `from`, which is
  // a boundary, and so after an even number of them.
  std::size_t indicators = before == regional_indicator ? 1 : 0;
  // Extended_Pictographic Extend* (GB11), and a ZWJ after it.
  bool pictographic = is_extended_pictographic(first.code_point);
  bool pictographic_zwj = false;
  // InCB=Linker InCB=Extend* (GB9c).
  bool linker = indic_conjunct_break_of(first.code_point) ==
                indic_conjunct_break::linker;

  std::size_t at = from + first.length;
  while (at < text.size()) {
    const decoded next = decode_front(text.substr(at));
    const grapheme_cluster_break after =
        grapheme_cluster_break_of(next.code_point);
    const indic_conjunct_break conjunct =
        indic_conjunct_break_of(next.code_point);
    const bool next_pictographic = is_extended_pictographic(next.code_point);
    bool joined = false;
    if (before == cr && after == lf)
      joined = true;  // GB3
    else if (is_control(before) || is_control(after))
      joined = false;  // GB4, GB5
    else if (before == l &&
             (after == l || after == v || after == lv || after == lvt))
      joined = true;  // GB6
    else if ((before == lv || before == v) && (after == v || after == t))
      joined = true;  // GB7
    else if ((before == lvt || before == t) && after == t)
      joined = true;  // GB8
    else if (after == extend || after == zwj || after == spacing_mark)
      joined = true;  // GB9, GB9a
    else if (before == prepend)
      joined = true;  // GB9b
    else if (linker && conjunct == indic_conjunct_break::consonant)
      joined = true;  // GB9c
    else if (pictographic_zwj && next_pictographic)
      joined = true;  // GB11
    else if (before == regional_indicator && after == regional_indicator)
      joined = indicators % 2 == 1;  // GB12, GB13
    if (!joined)
      return at;  // GB999, where nothing above joined them

    indicators = after == regional_indicator ? indicators + 1 : 0;
    pictographic_zwj = pictographic && after == zwj;
    pictographic = next_pictographic || (pictographic && after == extend);
    linker = conjunct == indic_conjunct_break::linker ||
             (linker && conjunct == indic_conjunct_break::extend);
    before = after;
    at += next.length;
  }
  return text.size();
}

// Whether there is a boundary at `at`, where a code point begins, inside the
// text. Decided between the code points on either side of it, looking back
// only as far as a rule asks.
template <utf8_char Char>
constexpr bool boundary_at(std::basic_string_view<Char> text,
                           std::size_t at) noexcept {
  using enum grapheme_cluster_break;
  const auto code_point = [&](std::size_t where) {
    return decode_front(text.substr(where)).code_point;
  };
  const std::size_t start = previous_start(text, at);
  const char32_t first = code_point(start);
  const char32_t second = code_point(at);
  const grapheme_cluster_break before = grapheme_cluster_break_of(first);
  const grapheme_cluster_break after = grapheme_cluster_break_of(second);
  if (before == cr && after == lf)
    return false;  // GB3
  if (is_control(before) || is_control(after))
    return true;  // GB4, GB5
  if (before == l && (after == l || after == v || after == lv || after == lvt))
    return false;  // GB6
  if ((before == lv || before == v) && (after == v || after == t))
    return false;  // GB7
  if ((before == lvt || before == t) && after == t)
    return false;  // GB8
  if (after == extend || after == zwj || after == spacing_mark)
    return false;  // GB9, GB9a
  if (before == prepend)
    return false;  // GB9b
  // GB9c: InCB=Linker InCB=Extend* x InCB=Consonant.
  if (indic_conjunct_break_of(second) == indic_conjunct_break::consonant) {
    std::size_t where = start;
    for (;;) {
      const indic_conjunct_break conjunct =
          indic_conjunct_break_of(code_point(where));
      if (conjunct == indic_conjunct_break::linker)
        return false;
      if (conjunct != indic_conjunct_break::extend || where == 0)
        break;
      where = previous_start(text, where);
    }
  }
  // GB11: Extended_Pictographic Extend* ZWJ x Extended_Pictographic.
  if (before == zwj && is_extended_pictographic(second)) {
    for (std::size_t where = start; where > 0;) {
      where = previous_start(text, where);
      const char32_t one = code_point(where);
      if (is_extended_pictographic(one))
        return false;
      if (grapheme_cluster_break_of(one) != extend)
        break;
    }
  }
  // GB12, GB13: no break inside a pair of regional indicators, counted back
  // to whatever is not one.
  if (before == regional_indicator && after == regional_indicator) {
    std::size_t indicators = 1;
    for (std::size_t where = start; where > 0;) {
      where = previous_start(text, where);
      if (grapheme_cluster_break_of(code_point(where)) != regional_indicator)
        break;
      ++indicators;
    }
    return indicators % 2 == 0;
  }
  return true;  // GB999
}

// The start of the cluster that ends at `from`, a boundary.
template <utf8_char Char>
constexpr std::size_t previous_boundary(std::basic_string_view<Char> text,
                                        std::size_t from) noexcept {
  if (from > text.size())
    from = text.size();
  if (from == 0)
    return 0;
  std::size_t at = previous_start(text, from);
  while (at > 0 && !boundary_at(text, at))
    at = previous_start(text, at);
  return at;
}

template <utf8_char Char>
constexpr bool is_boundary(std::basic_string_view<Char> text,
                           std::size_t at) noexcept {
  if (at == 0 || at == text.size())
    return true;  // GB1, GB2
  if (at > text.size() || !code_point_starts(text, at))
    return false;
  return boundary_at(text, at);
}

}  // namespace alef::detail

export namespace alef {

// The next grapheme cluster boundary after `from`, which is one: the end of
// the cluster that begins there.
template <utf8_text Text>
constexpr std::size_t next_grapheme_boundary(Text&& text,
                                             std::size_t from) noexcept {
  return detail::next_boundary(alef::as_utf8(text), from);
}

// The boundary before `from`, which is one: the start of the cluster that
// ends there.
template <utf8_text Text>
constexpr std::size_t prev_grapheme_boundary(Text&& text,
                                             std::size_t from) noexcept {
  return detail::previous_boundary(alef::as_utf8(text), from);
}

// Whether a grapheme cluster boundary is at byte `at` of `text`: never inside
// a code point, always at either end.
template <utf8_text Text>
constexpr bool is_grapheme_boundary(Text&& text, std::size_t at) noexcept {
  return detail::is_boundary(alef::as_utf8(text), at);
}

// The grapheme clusters of some UTF-8, each a piece of the text it was given,
// in either direction.
template <utf8_char Char>
class grapheme_view : public std::ranges::view_interface<grapheme_view<Char>> {
 public:
  class iterator {
   public:
    using value_type = std::basic_string_view<Char>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::bidirectional_iterator_tag;

    constexpr iterator() = default;

    constexpr std::basic_string_view<Char> operator*() const noexcept {
      return text_.substr(begin_, end_ - begin_);
    }

    constexpr iterator& operator++() noexcept {
      begin_ = end_;
      end_ = detail::next_boundary(text_, begin_);
      return *this;
    }
    constexpr iterator operator++(int) noexcept {
      iterator was = *this;
      ++*this;
      return was;
    }
    constexpr iterator& operator--() noexcept {
      end_ = begin_;
      begin_ = detail::previous_boundary(text_, begin_);
      return *this;
    }
    constexpr iterator operator--(int) noexcept {
      iterator was = *this;
      --*this;
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

    constexpr iterator(std::basic_string_view<Char> text,
                       std::size_t begin) noexcept
        : text_(text),
          begin_(begin),
          end_(detail::next_boundary(text, begin)) {}

    std::basic_string_view<Char> text_;
    std::size_t begin_ = 0;
    std::size_t end_ = 0;
  };

  constexpr grapheme_view() = default;
  constexpr explicit grapheme_view(std::basic_string_view<Char> text) noexcept
      : text_(text) {}

  constexpr iterator begin() const noexcept { return iterator(text_, 0); }
  constexpr iterator end() const noexcept {
    return iterator(text_, text_.size());
  }

 private:
  std::basic_string_view<Char> text_;
};

// graphemes(text), or text | graphemes.
struct graphemes_fn : std::ranges::range_adaptor_closure<graphemes_fn> {
  template <utf8_text Text>
    requires detail::lasting<Text>
  constexpr auto operator()(Text&& text) const noexcept {
    const auto bytes = alef::as_utf8(text);
    return grapheme_view<typename decltype(bytes)::value_type>(bytes);
  }
};
inline constexpr graphemes_fn graphemes{};

}  // namespace alef

// It holds a view of the text, not the text.
template <class Char>
inline constexpr bool
    std::ranges::enable_borrowed_range<alef::grapheme_view<Char>> = true;
