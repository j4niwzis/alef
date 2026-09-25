// Word boundaries: UAX #29, Unicode Text Segmentation, for Unicode 18.0.0,
// with Word_Break and Extended_Pictographic from the table of them.
//
// What comes apart here are the pieces between word boundaries: words, and
// the spaces and punctuation between them, each a piece of its own.
export module alef.word;

import std;
import alef.utf;
import alef.tables.word;

export namespace alef {

using alef::word_break;

constexpr word_break word_break_of(char32_t code_point) noexcept {
  return tables::word_properties_of(code_point).word_break();
}

}  // namespace alef

namespace alef::detail::word_rules {

constexpr word_break property_of(char32_t code_point) noexcept {
  return tables::word_properties_of(code_point).word_break();
}

// WB4: what is passed over after anything but the start of the text and a
// line break.
constexpr bool ignorable(word_break one) noexcept {
  return one == word_break::extend || one == word_break::format ||
         one == word_break::zwj;
}
constexpr bool line_break(word_break one) noexcept {
  return one == word_break::newline || one == word_break::cr ||
         one == word_break::lf;
}
constexpr bool ahletter(word_break one) noexcept {
  return one == word_break::aletter || one == word_break::hebrew_letter;
}
constexpr bool midnumletq(word_break one) noexcept {
  return one == word_break::midnumlet || one == word_break::single_quote;
}

// What the rules ask of the text before a place: the code point just before
// it; the last two code points WB4 does not pass over; and how many regional
// indicators in a row end with the last of them.
struct context {
  word_break just_before = word_break::other;
  std::optional<word_break> last;
  std::optional<word_break> before_last;
  std::size_t indicators = 0;

  // `one` after it. `start` if it begins the text, or what is read of it.
  constexpr void take(word_break one, bool start) noexcept {
    const bool passed_over = ignorable(one) && !start && !line_break(just_before);
    if (!passed_over) {
      before_last = last;
      last = one;
      indicators = one == word_break::regional_indicator ? indicators + 1 : 0;
    }
    just_before = one;
  }
};

// Whether the code point with `after` goes on with the word before it.
// `ahead` gives the next code point after it that WB4 does not pass over, and
// is asked only where a rule needs it: WB6, WB7b and WB12.
template <class Ahead>
constexpr bool joins(const context& before, tables::word_properties after,
                     Ahead&& ahead) {
  using enum word_break;
  const word_break r = after.word_break();
  if (before.just_before == cr && r == lf)
    return true;  // WB3
  if (line_break(before.just_before))
    return false;  // WB3a
  if (line_break(r))
    return false;  // WB3b
  if (before.just_before == zwj && after.pictographic())
    return true;  // WB3c
  if (before.just_before == wsegspace && r == wsegspace)
    return true;  // WB3d
  if (ignorable(r))
    return true;  // WB4
  if (!before.last)
    return false;
  const word_break l = *before.last;
  const std::optional<word_break> ll = before.before_last;
  if (ahletter(l) && ahletter(r))
    return true;  // WB5
  if (ahletter(l) && (r == midletter || midnumletq(r))) {
    const std::optional<word_break> rr = ahead();
    if (rr && ahletter(*rr))
      return true;  // WB6
  }
  if (ll && ahletter(*ll) && (l == midletter || midnumletq(l)) && ahletter(r))
    return true;  // WB7
  if (l == hebrew_letter && r == single_quote)
    return true;  // WB7a
  if (l == hebrew_letter && r == double_quote && ahead() == hebrew_letter)
    return true;  // WB7b
  if (ll == hebrew_letter && l == double_quote && r == hebrew_letter)
    return true;  // WB7c
  if (l == numeric && r == numeric)
    return true;  // WB8
  if (ahletter(l) && r == numeric)
    return true;  // WB9
  if (l == numeric && ahletter(r))
    return true;  // WB10
  if (ll == numeric && (l == midnum || midnumletq(l)) && r == numeric)
    return true;  // WB11
  if (l == numeric && (r == midnum || midnumletq(r)) && ahead() == numeric)
    return true;  // WB12
  if (l == katakana && r == katakana)
    return true;  // WB13
  if ((ahletter(l) || l == numeric || l == katakana || l == extendnumlet) &&
      r == extendnumlet)
    return true;  // WB13a
  if (l == extendnumlet && (ahletter(r) || r == numeric || r == katakana))
    return true;  // WB13b
  if (l == regional_indicator && r == regional_indicator)
    return before.indicators % 2 == 1;  // WB15, WB16
  return false;  // WB999
}

// The first code point from `at` that WB4 does not pass over, or nothing.
template <code_unit Unit, class I, class S>
constexpr std::optional<word_break> next_counted(I at, const S& last) {
  while (at != last) {
    bool well_formed = false;
    const word_break one = property_of(read<Unit>(at, last, well_formed));
    if (!ignorable(one))
      return one;
  }
  return std::nullopt;
}

// The end of the piece that begins at `from`, a boundary: the next boundary
// after it, or `last`. Read forwards, carrying what the rules ask of the text
// before instead of looking back for it; from a boundary, nothing before it
// is asked about.
template <code_unit Unit, class I, class S>
constexpr I next_boundary(I from, const S& last) {
  if (from == last)
    return from;
  bool well_formed = false;
  I at = from;
  context before;
  before.take(property_of(read<Unit>(at, last, well_formed)), true);
  while (at != last) {
    I here = at;
    const tables::word_properties after =
        tables::word_properties_of(read<Unit>(at, last, well_formed));
    if (!joins(before, after, [&] { return next_counted<Unit>(at, last); }))
      return here;
    before.take(after.word_break(), false);
  }
  return at;
}

// Where the code point before `at` begins, and its property.
template <code_unit Unit, class I>
constexpr std::pair<I, word_break> code_point_before(const I& first,
                                                     const I& at) {
  I where = step_back<Unit>(first, at);
  I reading = where;
  bool well_formed = false;
  const char32_t code_point = read<Unit>(reading, at, well_formed);
  return {where, property_of(code_point)};
}

// The code point WB4 counts that ends before `at`: going back over what it
// passes over, to what that was passed over into -- unless it began the text
// or came after a line break, where the first of it counts itself.
template <code_unit Unit, class I>
constexpr std::pair<I, word_break> counted_before(const I& first, const I& at) {
  auto [where, one] = code_point_before<Unit>(first, at);
  while (ignorable(one) && where != first) {
    const auto [earlier, property] = code_point_before<Unit>(first, where);
    if (line_break(property))
      break;
    where = earlier;
    one = property;
  }
  return {where, one};
}

// Whether there is a boundary at `at`, where a code point begins, between
// `first` and `bound`: the context of the rules looked back for, as far as
// they ask.
template <code_unit Unit, class I, class S>
constexpr bool boundary_at(const I& first, const I& at, const S& bound) {
  context before;
  before.just_before = code_point_before<Unit>(first, at).second;
  const auto [last_at, last] = counted_before<Unit>(first, at);
  before.last = last;
  if (last_at != first)
    before.before_last = counted_before<Unit>(first, last_at).second;
  if (last == word_break::regional_indicator) {
    std::size_t indicators = 1;
    for (I where = last_at; where != first;) {
      const auto [earlier, one] = counted_before<Unit>(first, where);
      if (one != word_break::regional_indicator)
        break;
      ++indicators;
      where = earlier;
    }
    before.indicators = indicators;
  }
  I after_at = at;
  bool well_formed = false;
  const tables::word_properties after =
      tables::word_properties_of(read<Unit>(after_at, bound, well_formed));
  return !joins(before, after,
                [&] { return next_counted<Unit>(after_at, bound); });
}

// The start of the piece that ends at `from`, a boundary.
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
    return true;  // WB1, WB2
  if (!starts<Unit>(first, at, last))
    return false;
  return boundary_at<Unit>(first, at, last);
}

// Whether all that decides the boundary at `p` has been read, when `e` is
// only where the text read so far ends: the code point at `p`, and the next
// one that WB4 counts after it -- the furthest the rules look ahead -- with a
// code unit more after that, so that none of them was cut off.
template <code_unit Unit, class I>
constexpr bool settled(I p, const I& e) {
  if (p == e)
    return false;
  bool well_formed = false;
  read<Unit>(p, e, well_formed);
  while (p != e)
    if (!ignorable(property_of(read<Unit>(p, e, well_formed))))
      return p != e;
  return false;
}

}  // namespace alef::detail::word_rules

export namespace alef {

// The next word boundary after `at`, which is one.
template <std::forward_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr I next_word_boundary(I at, S last) {
  return detail::word_rules::next_boundary<std::iter_value_t<I>>(std::move(at),
                                                                 last);
}

// The word boundary before `at`, which is one. `first` is where the text
// begins.
template <std::bidirectional_iterator I>
  requires code_unit<std::iter_value_t<I>>
constexpr I prev_word_boundary(I first, I at) {
  return detail::word_rules::previous_boundary<std::iter_value_t<I>>(
      first, std::move(at));
}

// Whether a word boundary is at `at`: never inside a code point, always at
// either end.
template <std::bidirectional_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr bool is_word_boundary(I first, I at, S last) {
  return detail::word_rules::is_boundary<std::iter_value_t<I>>(first, at, last);
}

// The pieces between word boundaries of text in any UTF, each the part of
// the text it was read from; bidirectional if the text is.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class word_view : public std::ranges::view_interface<word_view<V>> {
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
      end_ = detail::word_rules::next_boundary<From>(begin_, last_);
    }

    constexpr std::ranges::subrange<I> operator*() const { return {begin_, end_}; }

    constexpr iterator& operator++() {
      begin_ = end_;
      end_ = detail::word_rules::next_boundary<From>(begin_, last_);
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
      begin_ = detail::word_rules::previous_boundary<From>(first_, begin_);
      return *this;
    }
    constexpr iterator operator--(int)
      requires bidirectional
    {
      iterator was = *this;
      --*this;
      return was;
    }

    friend constexpr bool operator==(const iterator& one, const iterator& other) {
      return one.begin_ == other.begin_;
    }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) {
      return one.begin_ == one.last_;
    }

    // Where the piece begins in V.
    constexpr I base() const { return begin_; }

   private:
    [[no_unique_address]] std::conditional_t<bidirectional, I, detail::nothing>
        first_{};
    I begin_{};
    I end_{};
    [[no_unique_address]] S last_{};
  };

 public:
  word_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit word_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr auto begin() { return iterator<false>(base_, std::ranges::begin(base_)); }
  constexpr auto begin() const
    requires std::ranges::forward_range<const V>
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
    requires std::ranges::forward_range<const V>
  {
    if constexpr (std::ranges::common_range<const V>)
      return iterator<true>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }

 private:
  V base_ = V();
};

// The pieces between word boundaries of text that can be read only once --
// a stream, say. What is read is kept until the boundary after it is
// settled, and each piece is a string_view of it, good until the next.
template <std::ranges::view V>
  requires utf_range<V> && (!std::ranges::forward_range<V>)
class word_input_view : public std::ranges::view_interface<word_input_view<V>> {
  using Unit = detail::unit_of<V>;

 public:
  class iterator {
   public:
    using value_type = std::basic_string_view<Unit>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::input_iterator_tag;

    constexpr explicit iterator(word_input_view* view) : view_(view) {}
    iterator(iterator&&) = default;
    iterator& operator=(iterator&&) = default;

    constexpr value_type operator*() const { return view_->piece(); }
    constexpr iterator& operator++() {
      view_->advance();
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) {
      return one.view_->done_;
    }

   private:
    word_input_view* view_;
  };

  constexpr explicit word_input_view(V base) : base_(std::move(base)) {}

  constexpr iterator begin() {
    at_.emplace(std::ranges::begin(base_));
    find();
    return iterator(this);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  constexpr bool pull() {
    if (*at_ == std::ranges::end(base_)) {
      read_all_ = true;
      return false;
    }
    buffer_.push_back(static_cast<Unit>(**at_));
    ++*at_;
    return true;
  }
  // The piece at the start of what is kept: read on until its end is sure.
  constexpr void find() {
    if (buffer_.empty() && !pull()) {
      done_ = true;
      return;
    }
    for (;;) {
      const auto first = buffer_.cbegin();
      const auto last = buffer_.cend();
      const auto boundary = detail::word_rules::next_boundary<Unit>(first, last);
      if (read_all_ || detail::word_rules::settled<Unit>(boundary, last)) {
        end_ = static_cast<std::size_t>(boundary - first);
        return;
      }
      pull();
    }
  }
  constexpr void advance() {
    buffer_.erase(0, end_);
    end_ = 0;
    find();
  }
  constexpr std::basic_string_view<Unit> piece() const { return {buffer_.data(), end_}; }

  V base_;
  std::optional<std::ranges::iterator_t<V>> at_;
  std::basic_string<Unit> buffer_;
  std::size_t end_ = 0;
  bool read_all_ = false;
  bool done_ = false;
};

// text | words, or words(text): subranges of text read more than once, and
// kept pieces of text read once.
struct words_fn : std::ranges::range_adaptor_closure<words_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    using View = detail::all_of_t<Range>;
    if constexpr (std::ranges::forward_range<View>)
      return word_view<View>(detail::all_of(std::forward<Range>(range)));
    else
      return word_input_view<View>(detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr words_fn words{};

}  // namespace alef

template <class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::word_view<V>> =
    std::ranges::enable_borrowed_range<V>;
