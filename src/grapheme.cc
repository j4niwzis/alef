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
      : before_(grapheme_cluster_break_of(first)),
        odd_indicators_(before_ == grapheme_cluster_break::regional_indicator),
        pictographic_(is_extended_pictographic(first)),
        linker_(indic_conjunct_break_of(first) == indic_conjunct_break::linker) {}

  // Whether `next` goes on with the cluster, which takes it in if it does.
  constexpr bool joins(char32_t next) noexcept {
    using enum grapheme_cluster_break;
    const grapheme_cluster_break after = grapheme_cluster_break_of(next);
    const indic_conjunct_break conjunct = indic_conjunct_break_of(next);
    const bool next_pictographic = is_extended_pictographic(next);
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

// Whether there is a boundary at `at`, where a code point begins, between
// `first` and `bound`. Decided between the code points on either side of it,
// looking back only as far as a rule asks.
template <code_unit Unit, class I, class S>
constexpr bool boundary_at(const I& first, const I& at, const S& bound) {
  using enum grapheme_cluster_break;
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

// The grapheme clusters of text in any UTF.
//
// Over text that can be read only once -- a stream, say -- a cluster is a
// range of its own, which reads the code units of the cluster from the text
// as they are asked for, and the code point after them: that is how a
// cluster is known to have ended, and it is what the next one begins with.
// Stepping to the next cluster reads past whatever of this one was not asked
// for. So it goes one cluster at a time, as std::views::lazy_split does over
// such text: a cluster is read while it is the current one, and not after.
template <std::ranges::view V>
  requires utf_range<V>
class grapheme_view : public std::ranges::view_interface<grapheme_view<V>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

  // Where the reading is: the view's own, because the text is read once.
  struct reading {
    I next;
    S last;
    // The code point being given out, as code units, and how many of them
    // have been. None at the end of the text.
    std::array<Unit, 4> units{};
    std::uint8_t count = 0;
    std::uint8_t given = 0;
    // Whether that code point begins the next cluster rather than going on
    // with this one, and whether the clusters are all given out.
    bool ended = false;
    bool done = false;
    detail::cluster_rules rules{};

    constexpr reading(I first, S end)
        : next(std::move(first)), last(std::move(end)) {}

    // The next code point, read into `units`; none at the end of the text.
    constexpr std::optional<char32_t> read() {
      given = 0;
      count = 0;
      if (next == last)
        return std::nullopt;
      bool well_formed = false;
      return detail::read<Unit>(next, last, well_formed,
                                [this](Unit unit) { units[count++] = unit; });
    }

    // Past one code unit of the cluster. Past the last of a code point, the
    // next one is read, and the rules say whether it goes on with the
    // cluster.
    constexpr void advance() {
      if (++given < count)
        return;
      const std::optional<char32_t> code_point = read();
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

// Over text that can be read more than once, each cluster is the part of V
// it was read from, and the view is bidirectional if V is.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class grapheme_view<V> : public std::ranges::view_interface<grapheme_view<V>> {
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

// graphemes(text), or text | graphemes.
struct graphemes_fn : std::ranges::range_adaptor_closure<graphemes_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    return grapheme_view<detail::all_of_t<Range>>(
        detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr graphemes_fn graphemes{};

}  // namespace alef

// Only over text read more than once: over text read once, the clusters are
// read through the view.
template <class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::grapheme_view<V>> =
    std::ranges::forward_range<V> && std::ranges::enable_borrowed_range<V>;
