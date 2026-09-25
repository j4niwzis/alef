// Grapheme clusters: what a reader takes for one character.
//
// The rules are those of UAX #29, Unicode Text Segmentation, revision 49, for
// extended grapheme clusters, and the properties they are decided by are read
// from the Unicode Character Database of the same version:
// Grapheme_Cluster_Break from GraphemeBreakProperty.txt, Indic_Conjunct_Break
// from DerivedCoreProperties.txt, Extended_Pictographic from emoji-data.txt.
export module alef:grapheme;

import std;
import :utf;
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

// All the rules ask of a code point, in one byte: Grapheme_Cluster_Break in
// the low four bits, Indic_Conjunct_Break in the two above them, and whether
// it is Extended_Pictographic in the one above those. Zero is Other, None,
// and not.
struct packed {
  std::uint8_t bits = 0;

  constexpr grapheme_cluster_break grapheme_break() const noexcept {
    return static_cast<grapheme_cluster_break>(bits & 0x0F);
  }
  constexpr indic_conjunct_break conjunct_break() const noexcept {
    return static_cast<indic_conjunct_break>((bits >> 4) & 0x03);
  }
  constexpr bool pictographic() const noexcept { return (bits & 0x40) != 0; }
};

constexpr std::uint8_t pack(grapheme_cluster_break grapheme,
                            indic_conjunct_break conjunct,
                            bool is_pictographic) noexcept {
  return static_cast<std::uint8_t>(static_cast<unsigned>(grapheme) |
                                   static_cast<unsigned>(conjunct) << 4 |
                                   (is_pictographic ? 0x40u : 0u));
}

// The three merged: runs of code points with the same byte, in order, where
// it is not zero. A run ends wherever a range of any of the three begins or
// ends, and runs of the same byte side by side are one.
struct run {
  char32_t first = 0;
  char32_t last = 0;
  std::uint8_t bits = 0;
};

constexpr std::vector<run> runs() {
  std::vector<run> out;
  // Where each of the three is: the first of its ranges that does not end
  // before the code point. They are walked side by side, once.
  std::size_t in_break = 0;
  std::size_t in_conjunct = 0;
  std::size_t in_pictographic = 0;
  for (char32_t code_point = 0; code_point <= 0x10FFFF;) {
    // Where the next of the three may change.
    char32_t next = 0x110000;
    const auto value = [&](const auto& ranges, std::size_t& index,
                           auto otherwise) {
      while (index < ranges.size() && ranges[index].last < code_point)
        ++index;
      if (index == ranges.size())
        return otherwise;
      if (ranges[index].first > code_point) {
        next = std::min(next, ranges[index].first);
        return otherwise;
      }
      next = std::min(next, static_cast<char32_t>(ranges[index].last + 1));
      return ranges[index].value;
    };
    const std::uint8_t bits = pack(
        value(grapheme_break, in_break, grapheme_cluster_break::other),
        value(conjunct_break, in_conjunct, indic_conjunct_break::none),
        value(pictographic, in_pictographic, false));
    const auto last = static_cast<char32_t>(next - 1);
    if (bits != 0) {
      if (!out.empty() && out.back().last + 1 == code_point &&
          out.back().bits == bits)
        out.back().last = last;
      else
        out.push_back({code_point, last, bits});
    }
    code_point = next;
  }
  return out;
}

// A table of two stages, which is what makes a code point's properties two
// reads and no search: Unicode in blocks of 256 code points, each block the
// index of the block of bytes it is the same as, and those blocks, each kept
// once. For Unicode 18.0.0, 111 of them are all 4352 blocks of it.
inline constexpr std::size_t block_size = 256;
inline constexpr std::size_t block_count = 0x110000 / block_size;

struct layout {
  std::vector<std::size_t> index;
  std::vector<std::uint8_t> blocks;
};

constexpr layout lay_out() {
  const std::vector<run> all = runs();
  layout out;
  out.index.reserve(block_count);
  // A sum of each block kept, so that few are compared in full; and the
  // block that is all of one byte, for each byte one is all of.
  std::vector<std::uint32_t> sums;
  std::array<std::size_t, 256> all_of{};
  all_of.fill(block_count);
  std::array<std::uint8_t, block_size> bytes{};
  const auto keep = [&] {
    std::uint32_t sum = 0;
    for (const std::uint8_t byte : bytes)
      sum = sum * 31 + byte;
    for (std::size_t kept = 0; kept < sums.size(); ++kept)
      if (sums[kept] == sum &&
          std::ranges::equal(bytes, std::span(out.blocks)
                                        .subspan(kept * block_size, block_size)))
        return kept;
    sums.push_back(sum);
    out.blocks.insert(out.blocks.end(), bytes.begin(), bytes.end());
    return sums.size() - 1;
  };
  std::size_t at = 0;  // the first run that does not end before the block
  for (std::size_t block = 0; block < block_count; ++block) {
    const auto start = static_cast<char32_t>(block * block_size);
    const auto end = static_cast<char32_t>(start + block_size - 1);
    while (at < all.size() && all[at].last < start)
      ++at;
    // All of one byte: no run in it, or one run over all of it -- which is
    // most of Unicode, and is not written out to be found so.
    std::optional<std::uint8_t> same;
    if (at == all.size() || all[at].first > end)
      same = 0;
    else if (all[at].first <= start && all[at].last >= end)
      same = all[at].bits;
    if (same) {
      if (all_of[*same] == block_count) {
        bytes.fill(*same);
        all_of[*same] = keep();
      }
      out.index.push_back(all_of[*same]);
      continue;
    }
    bytes.fill(0);
    for (std::size_t one = at; one < all.size() && all[one].first <= end; ++one)
      for (char32_t code_point = std::max(all[one].first, start);
           code_point <= std::min(all[one].last, end); ++code_point)
        bytes[code_point - start] = all[one].bits;
    out.index.push_back(keep());
  }
  return out;
}

inline constexpr std::size_t distinct_blocks =
    lay_out().blocks.size() / block_size;
static_assert(distinct_blocks <= 65536);

using block_index =
    std::conditional_t<(distinct_blocks <= 256), std::uint8_t, std::uint16_t>;

struct two_stages {
  std::array<block_index, block_count> index;
  std::array<std::uint8_t, distinct_blocks * block_size> bytes;
};

inline constexpr two_stages table = [] {
  const layout laid = lay_out();
  two_stages out{};
  for (std::size_t block = 0; block < block_count; ++block)
    out.index[block] = static_cast<block_index>(laid.index[block]);
  std::ranges::copy(laid.blocks, out.bytes.begin());
  return out;
}();

constexpr packed properties_of(char32_t code_point) noexcept {
  if (code_point > 0x10FFFF)
    return {};
  return {table.bytes[std::size_t{table.index[code_point / block_size]} *
                          block_size +
                      code_point % block_size]};
}

}  // namespace alef::tables

export namespace alef {

constexpr grapheme_cluster_break grapheme_cluster_break_of(
    char32_t code_point) noexcept {
  return tables::properties_of(code_point).grapheme_break();
}

constexpr indic_conjunct_break indic_conjunct_break_of(
    char32_t code_point) noexcept {
  return tables::properties_of(code_point).conjunct_break();
}

constexpr bool is_extended_pictographic(char32_t code_point) noexcept {
  return tables::properties_of(code_point).pictographic();
}

}  // namespace alef

namespace alef::detail {

constexpr bool is_control(grapheme_cluster_break one) noexcept {
  using enum grapheme_cluster_break;
  return one == control || one == cr || one == lf;
}

// An optional that is not copied or moved with what holds it, as the
// standard's non-propagating-cache: a view over text that can be read only
// once keeps its place in the text, and a copy of the view is not there.
template <class T>
class non_propagating : public std::optional<T> {
 public:
  constexpr non_propagating() noexcept = default;
  constexpr non_propagating(const non_propagating&) noexcept
      : std::optional<T>() {}
  constexpr non_propagating(non_propagating&& other) noexcept
      : std::optional<T>() {
    other.reset();
  }
  constexpr non_propagating& operator=(const non_propagating& other) noexcept {
    if (this != &other)
      this->reset();
    return *this;
  }
  constexpr non_propagating& operator=(non_propagating&& other) noexcept {
    this->reset();
    other.reset();
    return *this;
  }
};

// What the rules ask of a cluster so far, carried forward from where it
// began instead of looked back for: enough to say whether the next code
// point goes on with it.
class cluster_rules {
 public:
  constexpr cluster_rules() noexcept = default;

  // A cluster that begins with `first`. Where a cluster begins is a
  // boundary, so nothing before it is asked about: the regional indicators
  // before a boundary are an even number of them, and neither GB9c nor GB11
  // reaches back past one.
  constexpr explicit cluster_rules(char32_t first) noexcept
      : cluster_rules(tables::properties_of(first)) {}

  // Whether `next` goes on with the cluster, which takes it in if it does.
  constexpr bool joins(char32_t next) noexcept {
    using enum grapheme_cluster_break;
    // An ASCII code point is CR, LF, a control or Other, and nothing else the
    // rules ask about: what they come to for it is said here, without them.
    if (next < 0x80) {
      const bool joined =
          before_ == cr ? next == U'\n'
                        : before_ == prepend && next >= 0x20 && next != 0x7F;
      if (!joined)
        return false;
      before_ = next == U'\n' ? lf : other;
      odd_indicators_ = pictographic_ = pictographic_zwj_ = linker_ = false;
      return true;
    }
    const tables::packed properties = tables::properties_of(next);
    const grapheme_cluster_break after = properties.grapheme_break();
    const indic_conjunct_break conjunct = properties.conjunct_break();
    const bool next_pictographic = properties.pictographic();
    bool joined = false;
    if (before_ == cr && after == lf)
      joined = true;  // GB3
    else if (is_control(before_) || is_control(after))
      joined = false;  // GB4, GB5
    else if (before_ == l &&
             (after == l || after == v || after == lv || after == lvt))
      joined = true;  // GB6
    else if ((before_ == lv || before_ == v) && (after == v || after == t))
      joined = true;  // GB7
    else if ((before_ == lvt || before_ == t) && after == t)
      joined = true;  // GB8
    else if (after == extend || after == zwj || after == spacing_mark)
      joined = true;  // GB9, GB9a
    else if (before_ == prepend)
      joined = true;  // GB9b
    else if (linker_ && conjunct == indic_conjunct_break::consonant)
      joined = true;  // GB9c
    else if (pictographic_zwj_ && next_pictographic)
      joined = true;  // GB11
    else if (before_ == regional_indicator && after == regional_indicator)
      joined = odd_indicators_;  // GB12, GB13
    if (!joined)
      return false;  // GB999, where nothing above joined them

    odd_indicators_ = after == regional_indicator && !odd_indicators_;
    pictographic_zwj_ = pictographic_ && after == zwj;
    pictographic_ = next_pictographic || (pictographic_ && after == extend);
    linker_ = conjunct == indic_conjunct_break::linker ||
              (linker_ && conjunct == indic_conjunct_break::extend);
    before_ = after;
    return true;
  }

 private:
  // One code point's properties, read once for all three.
  constexpr explicit cluster_rules(tables::packed first) noexcept
      : before_(first.grapheme_break()),
        odd_indicators_(before_ == grapheme_cluster_break::regional_indicator),
        pictographic_(first.pictographic()),
        linker_(first.conjunct_break() == indic_conjunct_break::linker) {}

  grapheme_cluster_break before_ = grapheme_cluster_break::other;
  // An odd number of regional indicators in a row ends it (GB12, GB13).
  bool odd_indicators_ = false;
  // It ends in Extended_Pictographic Extend* (GB11), or in that and a ZWJ.
  bool pictographic_ = false;
  bool pictographic_zwj_ = false;
  // It ends in InCB=Linker InCB=Extend* (GB9c).
  bool linker_ = false;
};

// The end of the cluster that begins at `from`, a boundary: the next boundary
// after it, or `last`.
template <code_unit Unit, class I, class S>
constexpr I next_boundary(I from, const S& last) {
  if (from == last)
    return from;
  // Most text is mostly ASCII, and between two ASCII code points the rules
  // come to a boundary, but inside CR LF -- after which there is one, LF
  // being a control. So that much is said without reading the rules.
  if (const auto unit = static_cast<std::uint32_t>(*from); unit < 0x80) {
    I after = std::ranges::next(from);
    if (after == last)
      return after;
    if (const auto next = static_cast<std::uint32_t>(*after); next < 0x80)
      return unit == U'\r' && next == U'\n' ? std::ranges::next(after) : after;
  }
  bool well_formed = false;
  I at = from;
  cluster_rules rules(read<Unit>(at, last, well_formed));
  while (at != last) {
    I here = at;
    if (!rules.joins(read<Unit>(at, last, well_formed)))
      return here;
  }
  return at;
}

// Text read once, a code point at a time: where the reading is, and the code
// units of the code point read last, which in such text are all that is left
// of them.
template <class Unit, class I, class S>
struct code_point_reader {
  I next;
  S last;
  std::array<Unit, 4> units{};
  std::uint8_t count = 0;

  constexpr code_point_reader(I first, S end)
      : next(std::move(first)), last(std::move(end)) {}

  // The next code point, read into `units`; none at the end of the text.
  constexpr std::optional<char32_t> read() {
    count = 0;
    if (next == last)
      return std::nullopt;
    bool well_formed = false;
    return detail::read<Unit>(next, last, well_formed,
                              [this](Unit unit) { units[count++] = unit; });
  }
};

// An adaptor with an option said, as a closure: text | graphemes(owning<>).
template <class Adaptor, class Option>
struct with_option
    : std::ranges::range_adaptor_closure<with_option<Adaptor, Option>> {
  template <class Range>
    requires std::invocable<const Adaptor&, Range, Option>
  constexpr auto operator()(Range&& range) const {
    return Adaptor{}(std::forward<Range>(range), Option{});
  }
};

// Whether there is a boundary at `at`, where a code point begins, between
// `first` and `bound`. Decided between the code points on either side of it,
// looking back only as far as a rule asks.
template <code_unit Unit, class I, class S>
constexpr bool boundary_at(const I& first, const I& at, const S& bound) {
  using enum grapheme_cluster_break;
  // ASCII on both sides: a boundary, but inside CR LF.
  if (const auto unit = static_cast<std::uint32_t>(*at),
      before = static_cast<std::uint32_t>(*std::ranges::prev(at));
      unit < 0x80 && before < 0x80)
    return !(before == U'\r' && unit == U'\n');
  const auto code_point = [&](I where) {
    bool well_formed = false;
    return read<Unit>(where, bound, well_formed);
  };
  const I start = step_back<Unit>(first, at);
  const char32_t one = code_point(start);
  const char32_t two = code_point(at);
  const grapheme_cluster_break before = grapheme_cluster_break_of(one);
  const grapheme_cluster_break after = grapheme_cluster_break_of(two);
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
  if (indic_conjunct_break_of(two) == indic_conjunct_break::consonant) {
    I where = start;
    for (;;) {
      const indic_conjunct_break conjunct =
          indic_conjunct_break_of(code_point(where));
      if (conjunct == indic_conjunct_break::linker)
        return false;
      if (conjunct != indic_conjunct_break::extend || where == first)
        break;
      where = step_back<Unit>(first, where);
    }
  }
  // GB11: Extended_Pictographic Extend* ZWJ x Extended_Pictographic.
  if (before == zwj && is_extended_pictographic(two)) {
    for (I where = start; where != first;) {
      where = step_back<Unit>(first, where);
      const char32_t earlier = code_point(where);
      if (is_extended_pictographic(earlier))
        return false;
      if (grapheme_cluster_break_of(earlier) != extend)
        break;
    }
  }
  // GB12, GB13: no break inside a pair of regional indicators, counted back
  // to whatever is not one.
  if (before == regional_indicator && after == regional_indicator) {
    std::size_t indicators = 1;
    for (I where = start; where != first;) {
      where = step_back<Unit>(first, where);
      if (grapheme_cluster_break_of(code_point(where)) != regional_indicator)
        break;
      ++indicators;
    }
    return indicators % 2 == 0;
  }
  return true;  // GB999
}

// The start of the cluster that ends at `from`, a boundary.
template <code_unit Unit, class I>
constexpr I previous_boundary(const I& first, I from) {
  if (from == first)
    return from;
  I at = step_back<Unit>(first, from);
  while (at != first && !boundary_at<Unit>(first, at, from))
    at = step_back<Unit>(first, at);
  return at;
}

template <code_unit Unit, class I, class S>
constexpr bool is_boundary(const I& first, const I& at, const S& last) {
  if (at == first || at == last)
    return true;  // GB1, GB2
  if (!starts<Unit>(first, at, last))
    return false;
  return boundary_at<Unit>(first, at, last);
}

}  // namespace alef::detail

export namespace alef {

// The next grapheme cluster boundary after `at`, which is one: the end of the
// cluster that begins there.
template <std::forward_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr I next_grapheme_boundary(I at, S last) {
  return detail::next_boundary<std::iter_value_t<I>>(std::move(at), last);
}

// The boundary before `at`, which is one: the start of the cluster that ends
// there. `first` is where the text begins.
template <std::bidirectional_iterator I>
  requires code_unit<std::iter_value_t<I>>
constexpr I prev_grapheme_boundary(I first, I at) {
  return detail::previous_boundary<std::iter_value_t<I>>(first, std::move(at));
}

// Whether a grapheme cluster boundary is at `at`: never inside a code point,
// always at either end.
template <std::bidirectional_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr bool is_grapheme_boundary(I first, I at, S last) {
  return detail::is_boundary<std::iter_value_t<I>>(first, at, last);
}

// One grapheme cluster of its own: its code units, kept in the object itself
// up to Inline of them, and on the heap past that. A cluster has no bound on
// its length -- a base and any number of marks after it is one -- so the heap
// is always there to fall back on; Inline is for the clusters text is mostly
// made of. By default 32 bytes' worth: 32 code units of UTF-8, 16 of UTF-16,
// 8 of UTF-32, which a family of four as one emoji fits in.
template <code_unit Unit, std::size_t Inline = 32 / sizeof(Unit)>
class grapheme {
 public:
  using value_type = Unit;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using const_iterator = const Unit*;
  using iterator = const_iterator;

  static constexpr std::size_t inline_capacity = Inline;

  constexpr grapheme() noexcept = default;
  constexpr explicit grapheme(std::basic_string_view<Unit> units) {
    append(units.data(), units.size());
  }
  constexpr grapheme(const grapheme&) = default;
  constexpr grapheme(grapheme&& other) noexcept
      : inline_(other.inline_),
        size_(std::exchange(other.size_, 0)),
        heap_(std::move(other.heap_)) {}
  constexpr grapheme& operator=(const grapheme&) = default;
  constexpr grapheme& operator=(grapheme&& other) noexcept {
    if (this != &other) {
      inline_ = other.inline_;
      size_ = std::exchange(other.size_, 0);
      heap_ = std::move(other.heap_);
      other.heap_.clear();
    }
    return *this;
  }
  constexpr ~grapheme() = default;

  constexpr const Unit* data() const noexcept {
    return heap_.empty() ? inline_.data() : heap_.data();
  }
  constexpr std::size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr const Unit* begin() const noexcept { return data(); }
  constexpr const Unit* end() const noexcept { return data() + size_; }
  constexpr Unit operator[](std::size_t at) const noexcept { return data()[at]; }

  constexpr std::basic_string_view<Unit> view() const noexcept {
    return {data(), size_};
  }
  constexpr operator std::basic_string_view<Unit>() const noexcept {
    return view();
  }

  // Whether the code units are in the object itself rather than on the heap.
  constexpr bool is_inline() const noexcept { return heap_.empty(); }

  friend constexpr bool operator==(const grapheme& one,
                                   const grapheme& other) noexcept {
    return one.view() == other.view();
  }
  friend constexpr bool operator==(const grapheme& one,
                                   std::basic_string_view<Unit> other) noexcept {
    return one.view() == other;
  }
  friend constexpr auto operator<=>(const grapheme& one,
                                    const grapheme& other) noexcept {
    return one.view() <=> other.view();
  }

  // Made empty, keeping what was allocated for the next cluster.
  constexpr void clear() noexcept {
    size_ = 0;
    heap_.clear();
  }
  constexpr void append(const Unit* units, std::size_t count) {
    if (heap_.empty() && size_ + count <= Inline) {
      std::copy_n(units, count, inline_.data() + size_);
    } else {
      if (heap_.empty())
        heap_.assign(inline_.data(), inline_.data() + size_);
      heap_.insert(heap_.end(), units, units + count);
    }
    size_ += count;
  }

 private:
  std::array<Unit, Inline> inline_{};
  std::size_t size_ = 0;
  std::vector<Unit> heap_;
};

// Asks graphemes for graphemes of their own over any text -- text that is
// expensive to read more than once, or to read at all -- so each code unit
// is read once and what was read is kept. owning<N> keeps N code units in
// each grapheme itself; owning<> 32 bytes' worth of the text's UTF.
template <std::size_t Inline = std::dynamic_extent>
struct owning_t {};
template <std::size_t Inline = std::dynamic_extent>
inline constexpr owning_t<Inline> owning{};

// The same, with the room in each grapheme said in bytes: as many code units
// as fit in Bytes, whatever the text's UTF.
template <std::size_t Bytes>
struct owning_in_bytes_t {};
template <std::size_t Bytes>
inline constexpr owning_in_bytes_t<Bytes> owning_in_bytes{};

// The grapheme clusters of text that can be read more than once, each the
// part of the text it was read from; bidirectional if the text is.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class grapheme_view : public std::ranges::view_interface<grapheme_view<V>> {
  using From = detail::unit_of<V>;

  template <bool Const>
  class iterator {
    using Base = std::conditional_t<Const, const V, V>;
    using I = std::ranges::iterator_t<Base>;
    using S = std::ranges::sentinel_t<Base>;
    static constexpr bool bidirectional = std::ranges::bidirectional_range<Base>;

   public:
    using value_type = std::ranges::subrange<I>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept =
        std::conditional_t<bidirectional, std::bidirectional_iterator_tag,
                           std::forward_iterator_tag>;

    iterator()
      requires std::default_initializable<I>
    = default;

    constexpr iterator(Base& base, I at)
        : begin_(std::move(at)), last_(std::ranges::end(base)) {
      if constexpr (bidirectional)
        first_ = std::ranges::begin(base);
      end_ = detail::next_boundary<From>(begin_, last_);
    }

    constexpr std::ranges::subrange<I> operator*() const {
      return {begin_, end_};
    }

    constexpr iterator& operator++() {
      begin_ = end_;
      end_ = detail::next_boundary<From>(begin_, last_);
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator was = *this;
      ++*this;
      return was;
    }
    constexpr iterator& operator--()
      requires bidirectional
    {
      end_ = begin_;
      begin_ = detail::previous_boundary<From>(first_, begin_);
      return *this;
    }
    constexpr iterator operator--(int)
      requires bidirectional
    {
      iterator was = *this;
      --*this;
      return was;
    }

    friend constexpr bool operator==(const iterator& one,
                                     const iterator& other) {
      return one.begin_ == other.begin_;
    }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) {
      return one.begin_ == one.last_;
    }

    // Where the cluster begins in V.
    constexpr I base() const { return begin_; }

   private:
    [[no_unique_address]] std::conditional_t<bidirectional, I, detail::nothing>
        first_{};
    I begin_{};
    I end_{};
    [[no_unique_address]] S last_{};
  };

 public:
  grapheme_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit grapheme_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr auto begin() {
    return iterator<false>(base_, std::ranges::begin(base_));
  }
  constexpr auto begin() const
    requires utf_range<const V> && std::ranges::forward_range<const V>
  {
    return iterator<true>(base_, std::ranges::begin(base_));
  }
  constexpr auto end() {
    if constexpr (std::ranges::common_range<V>)
      return iterator<false>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }
  constexpr auto end() const
    requires utf_range<const V> && std::ranges::forward_range<const V>
  {
    if constexpr (std::ranges::common_range<const V>)
      return iterator<true>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }

 private:
  V base_ = V();
};

// The grapheme clusters of text in any UTF, one at a time, each a range of
// its own that reads the code units of the cluster from the text as they
// are asked for -- and the code point after them: that is how a cluster is
// known to have ended, and it is what the next one begins with. Stepping to
// the next cluster reads past whatever of this one was not asked for. A
// cluster is read while it is the current one and not after, as
// std::views::lazy_split goes, and one of any length is read in the same few
// bytes, which is what text nobody vouches for is to be read with.
template <std::ranges::view V>
  requires utf_range<V>
class lazy_grapheme_view
    : public std::ranges::view_interface<lazy_grapheme_view<V>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

  // Where the reading is: the view's own, because the text is read once.
  struct reading : detail::code_point_reader<Unit, I, S> {
    using detail::code_point_reader<Unit, I, S>::code_point_reader;

    // How many code units of the code point read last have been given out;
    // whether it begins the next cluster rather than going on with this one;
    // whether the clusters are all given out.
    std::uint8_t given = 0;
    bool ended = false;
    bool done = false;
    detail::cluster_rules rules{};

    // Past one code unit of the cluster. Past the last of a code point, the
    // next one is read, and the rules say whether it goes on with the
    // cluster.
    constexpr void advance() {
      if (++given < this->count)
        return;
      given = 0;
      const std::optional<char32_t> code_point = this->read();
      if (!code_point) {
        ended = true;
      } else if (!rules.joins(*code_point)) {
        ended = true;
        rules = detail::cluster_rules(*code_point);
      }
    }
  };

 public:
  // One cluster: its code units, read from the text as they are asked for.
  class cluster : public std::ranges::view_interface<cluster> {
   public:
    class iterator {
     public:
      using value_type = Unit;
      using difference_type = std::ptrdiff_t;

      iterator() = default;
      constexpr explicit iterator(reading* at) noexcept : at_(at) {}

      constexpr Unit operator*() const { return at_->units[at_->given]; }
      constexpr iterator& operator++() {
        at_->advance();
        return *this;
      }
      constexpr void operator++(int) { at_->advance(); }
      friend constexpr bool operator==(const iterator& one,
                                       std::default_sentinel_t) noexcept {
        return one.at_->ended;
      }

     private:
      reading* at_ = nullptr;
    };

    cluster() = default;
    constexpr explicit cluster(reading* at) noexcept : at_(at) {}
    constexpr iterator begin() const noexcept { return iterator(at_); }
    constexpr std::default_sentinel_t end() const noexcept { return {}; }

   private:
    reading* at_ = nullptr;
  };

  class iterator {
   public:
    using value_type = cluster;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr explicit iterator(reading* at) noexcept : at_(at) {}

    constexpr cluster operator*() const noexcept { return cluster(at_); }
    constexpr iterator& operator++() {
      while (!at_->ended)
        at_->advance();
      if (at_->count == 0)
        at_->done = true;
      else
        at_->ended = false;
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) noexcept {
      return one.at_->done;
    }

   private:
    reading* at_ = nullptr;
  };

  lazy_grapheme_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit lazy_grapheme_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr iterator begin() {
    reading& at =
        reading_.emplace(std::ranges::begin(base_), std::ranges::end(base_));
    if (const std::optional<char32_t> first = at.read())
      at.rules = detail::cluster_rules(*first);
    else
      at.ended = at.done = true;
    return iterator(&at);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
  detail::non_propagating<reading> reading_;
};

// The grapheme clusters of text in any UTF, each a grapheme of its own: read
// whole into the view when it is stepped to, from each code unit read once,
// and given out as a reference to it until the next -- a copy is the
// caller's to keep. What graphemes gives over text read once, and over any
// text asked with owning<>.
template <std::ranges::view V,
          std::size_t Inline = 32 / sizeof(detail::unit_of<V>)>
  requires utf_range<V>
class owning_grapheme_view
    : public std::ranges::view_interface<owning_grapheme_view<V, Inline>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

  // Where the reading is, and the cluster read last.
  struct reading : detail::code_point_reader<Unit, I, S> {
    using detail::code_point_reader<Unit, I, S>::code_point_reader;

    detail::cluster_rules rules{};
    grapheme<Unit, Inline> current;
    bool done = false;

    // The next cluster, into `current`: the code point read ahead, and every
    // one after it that goes on with it.
    constexpr void next_cluster() {
      current.clear();
      if (this->count == 0) {
        done = true;
        return;
      }
      for (;;) {
        current.append(this->units.data(), this->count);
        const std::optional<char32_t> code_point = this->read();
        if (!code_point)
          return;
        if (!rules.joins(*code_point)) {
          rules = detail::cluster_rules(*code_point);
          return;
        }
      }
    }
  };

 public:
  class iterator {
   public:
    using value_type = grapheme<Unit, Inline>;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr explicit iterator(reading* at) noexcept : at_(at) {}

    constexpr const value_type& operator*() const noexcept {
      return at_->current;
    }
    constexpr iterator& operator++() {
      at_->next_cluster();
      return *this;
    }
    constexpr void operator++(int) { at_->next_cluster(); }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) noexcept {
      return one.at_->done;
    }

   private:
    reading* at_ = nullptr;
  };

  owning_grapheme_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit owning_grapheme_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr iterator begin() {
    reading& at =
        reading_.emplace(std::ranges::begin(base_), std::ranges::end(base_));
    if (const std::optional<char32_t> first = at.read())
      at.rules = detail::cluster_rules(*first);
    at.next_cluster();
    return iterator(&at);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
  detail::non_propagating<reading> reading_;
};

// text | graphemes, or graphemes(text): over text that can be read more than
// once, pieces of it; over text read once, graphemes of their own. And
// text | graphemes(owning<>), or graphemes(text, owning<>): graphemes of
// their own over any text -- and owning_in_bytes<N> in place of owning<N>.
struct graphemes_fn : std::ranges::range_adaptor_closure<graphemes_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    using View = detail::all_of_t<Range>;
    if constexpr (std::ranges::forward_range<View>)
      return grapheme_view<View>(detail::all_of(std::forward<Range>(range)));
    else
      return owning_grapheme_view<View>(
          detail::all_of(std::forward<Range>(range)));
  }

  template <std::ranges::viewable_range Range, std::size_t Inline>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range, owning_t<Inline>) const {
    using View = detail::all_of_t<Range>;
    constexpr std::size_t units = Inline == std::dynamic_extent
                                      ? 32 / sizeof(detail::unit_of<View>)
                                      : Inline;
    return owning_grapheme_view<View, units>(
        detail::all_of(std::forward<Range>(range)));
  }

  template <std::ranges::viewable_range Range, std::size_t Bytes>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range, owning_in_bytes_t<Bytes>) const {
    using View = detail::all_of_t<Range>;
    return owning_grapheme_view<View, Bytes / sizeof(detail::unit_of<View>)>(
        detail::all_of(std::forward<Range>(range)));
  }

  template <std::size_t Inline>
  constexpr auto operator()(owning_t<Inline>) const noexcept {
    return detail::with_option<graphemes_fn, owning_t<Inline>>{};
  }
  template <std::size_t Bytes>
  constexpr auto operator()(owning_in_bytes_t<Bytes>) const noexcept {
    return detail::with_option<graphemes_fn, owning_in_bytes_t<Bytes>>{};
  }
};
inline constexpr graphemes_fn graphemes{};

// text | lazy_graphemes, or lazy_graphemes(text).
struct lazy_graphemes_fn
    : std::ranges::range_adaptor_closure<lazy_graphemes_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    return lazy_grapheme_view<detail::all_of_t<Range>>(
        detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr lazy_graphemes_fn lazy_graphemes{};

}  // namespace alef

// Pieces of the text, which outlive the view.
template <class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::grapheme_view<V>> =
    std::ranges::enable_borrowed_range<V>;
