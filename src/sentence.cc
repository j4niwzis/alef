// SPDX-License-Identifier: AGPL-3.0-only
// Sentence boundaries: UAX #29, Unicode Text Segmentation, for Unicode 18.0.0,
// rules SB1 to SB998, with Sentence_Break from the table of it.
//
// What comes apart here are sentences, each with the spaces after it and the
// paragraph separator that ends it, if one does.
export module alef.sentence;

import std;
import alef.utf;
import alef.tables.sentence;

export namespace alef {

using alef::sentence_break;

constexpr sentence_break sentence_break_of(char32_t code_point) noexcept {
  return tables::sentence_break_of(code_point);
}

}  // namespace alef

namespace alef::detail::sentence_rules {

using enum sentence_break;

constexpr bool ignorable(sentence_break one) noexcept {
  return one == extend || one == format;  // SB5
}
constexpr bool paragraph_separator(sentence_break one) noexcept {
  return one == sep || one == cr || one == lf;
}
constexpr bool terminal(sentence_break one) noexcept {
  return one == aterm || one == sterm;
}

template <code_unit Unit, class I, class S>
constexpr sentence_break property_at(I at, const S& last) {
  bool well_formed = false;
  return tables::sentence_break_of(read<Unit>(at, last, well_formed));
}

// Just past a paragraph separator that `at` is just past: past the LF too
// of a CR LF (SB3), for after a separator there is always a boundary (SB4).
template <code_unit Unit, class I, class S>
constexpr I past_separator(sentence_break one, I at, const S& last) {
  if (one == cr && at != last) {
    I after = at;
    bool well_formed = false;
    if (tables::sentence_break_of(read<Unit>(after, last, well_formed)) == lf)
      return after;
  }
  return at;
}

// SB8's look ahead: from `at`, past what is none of OLetter, Upper, Lower,
// ParaSep and SATerm -- and past Extend and Format (SB5) -- a Lower.
template <code_unit Unit, class I, class S>
constexpr bool lower_ahead(I at, const S& last) {
  bool well_formed = false;
  while (at != last) {
    const sentence_break one = tables::sentence_break_of(read<Unit>(at, last, well_formed));
    if (one == lower)
      return true;
    if (one == oletter || one == upper || paragraph_separator(one) || terminal(one))
      return false;
  }
  return false;
}

// Where a sentence has got to after its last code point that counts: in the
// middle of it, or just after SATerm, then Close*, then Sp*.
enum class after : std::uint8_t { text, terminal, close, space };

// The end of the sentence that begins at `from`, a boundary: the next
// boundary after it, or `last`. Read forwards, carrying what the rules ask
// of the text before -- SATerm Close* Sp*, what came before an ATerm -- and
// looking ahead only as far as SB8 asks.
template <code_unit Unit, class I, class S>
constexpr I next_boundary(I from, const S& last) {
  if (from == last)
    return from;
  bool well_formed = false;
  I at = from;
  sentence_break previous = tables::sentence_break_of(read<Unit>(at, last, well_formed));
  if (paragraph_separator(previous))
    return past_separator<Unit>(previous, at, last);
  after where = after::text;
  bool after_aterm = false;
  sentence_break before_terminal = other;
  const auto take = [&](sentence_break one, sentence_break before) {
    if (terminal(one)) {
      where = after::terminal;
      after_aterm = one == aterm;
      before_terminal = before;
    } else if (one == close && (where == after::terminal || where == after::close)) {
      where = after::close;
    } else if (one == sp && where != after::text) {
      where = after::space;
    } else {
      where = after::text;
    }
  };
  take(previous, other);
  while (at != last) {
    const I here = at;
    const sentence_break one = tables::sentence_break_of(read<Unit>(at, last, well_formed));
    if (ignorable(one))
      continue;  // SB5
    if (where != after::text) {
      const bool no_boundary =
          (where == after::terminal && after_aterm && one == numeric) ||  // SB6
          (where == after::terminal && after_aterm && one == upper &&
           (before_terminal == upper || before_terminal == lower)) ||    // SB7
          (after_aterm && lower_ahead<Unit>(here, last)) ||              // SB8
          one == scontinue || terminal(one) ||                           // SB8a
          ((where == after::terminal || where == after::close) &&
           (one == close || one == sp || paragraph_separator(one))) ||   // SB9
          one == sp || paragraph_separator(one);                         // SB10
      if (!no_boundary)
        return here;  // SB11
    }
    if (paragraph_separator(one))
      return past_separator<Unit>(one, at, last);  // SB4
    take(one, previous);
    previous = one;
  }
  return at;  // SB2
}

// Back from `at` to where a paragraph separator ends -- after one there is
// always a boundary (SB4) -- or to `first`. `at` is not inside a CR LF.
template <code_unit Unit, class I>
constexpr I paragraph_start(const I& first, I at) {
  while (at != first) {
    const I before = step_back<Unit>(first, at);
    if (paragraph_separator(property_at<Unit>(before, at)))
      break;
    at = before;
  }
  return at;
}

// The start of the sentence that ends at `from`, a boundary: forwards from
// the start of its paragraph to the last boundary before `from`. What comes
// after `from` cannot move a boundary before it, so the text read ends there.
template <code_unit Unit, class I>
constexpr I previous_boundary(const I& first, I from) {
  if (from == first)
    return from;
  I at = step_back<Unit>(first, from);
  if (at != first && property_at<Unit>(at, from) == lf) {
    const I before = step_back<Unit>(first, at);
    if (property_at<Unit>(before, at) == cr)
      at = before;
  }
  at = paragraph_start<Unit>(first, at);
  for (;;) {
    const I next = next_boundary<Unit>(at, from);
    if (next == from)
      return at;
    at = next;
  }
}

template <code_unit Unit, class I, class S>
constexpr bool is_boundary(const I& first, const I& at, const S& last) {
  if (at == first || at == last)
    return true;  // SB1, SB2
  if (!starts<Unit>(first, at, last))
    return false;
  const I before = step_back<Unit>(first, at);
  if (property_at<Unit>(before, at) == cr && property_at<Unit>(at, last) == lf)
    return false;  // SB3
  I from = paragraph_start<Unit>(first, at);
  bool well_formed = false;
  while (from != at) {
    const I next = next_boundary<Unit>(from, last);
    for (I inside = from; inside != next;) {
      read<Unit>(inside, last, well_formed);
      if (inside == at)
        return inside == next;
    }
    from = next;
  }
  return true;
}

// Whether all that decides the boundary at `p` has been read, when `e` is
// only where the text read so far ends. SB8 looks ahead from it past
// anything but OLetter, Upper, Lower, ParaSep and SATerm, so up to the first
// of those, with a code unit more after it, so that it was not cut off.
template <code_unit Unit, class I>
constexpr bool settled(I p, const I& e) {
  bool well_formed = false;
  while (p != e) {
    const sentence_break one = tables::sentence_break_of(read<Unit>(p, e, well_formed));
    if (one == lower || one == oletter || one == upper || paragraph_separator(one) || terminal(one))
      return p != e;
  }
  return false;
}

}  // namespace alef::detail::sentence_rules

export namespace alef {

// The next sentence boundary after `at`, which is one.
template <std::forward_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr I next_sentence_boundary(I at, S last) {
  return detail::sentence_rules::next_boundary<std::iter_value_t<I>>(std::move(at), last);
}

// The sentence boundary before `at`, which is one. `first` is where the text
// begins.
template <std::bidirectional_iterator I>
  requires code_unit<std::iter_value_t<I>>
constexpr I prev_sentence_boundary(I first, I at) {
  return detail::sentence_rules::previous_boundary<std::iter_value_t<I>>(first, std::move(at));
}

// Whether a sentence boundary is at `at`: never inside a code point, always
// at the start and the end of the text.
template <std::bidirectional_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr bool is_sentence_boundary(I first, I at, S last) {
  return detail::sentence_rules::is_boundary<std::iter_value_t<I>>(first, at, last);
}

// The sentences of text in any UTF, each the part of the text it was read
// from; bidirectional if the text is.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class sentence_view : public std::ranges::view_interface<sentence_view<V>> {
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
      end_ = detail::sentence_rules::next_boundary<From>(begin_, last_);
    }

    constexpr std::ranges::subrange<I> operator*() const { return {begin_, end_}; }

    constexpr iterator& operator++() {
      begin_ = end_;
      end_ = detail::sentence_rules::next_boundary<From>(begin_, last_);
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
      begin_ = detail::sentence_rules::previous_boundary<From>(first_, begin_);
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

    // Where the sentence begins in V.
    constexpr I base() const { return begin_; }

   private:
    [[no_unique_address]] std::conditional_t<bidirectional, I, detail::nothing>
        first_{};
    I begin_{};
    I end_{};
    [[no_unique_address]] S last_{};
  };

 public:
  sentence_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit sentence_view(V base) : base_(std::move(base)) {}

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

// The sentences of text that can be read only once -- a stream, say. What
// is read is kept until the boundary after it is settled, and each sentence
// is a string_view of it, good until the next.
template <std::ranges::view V>
  requires utf_range<V> && (!std::ranges::forward_range<V>)
class sentence_input_view : public std::ranges::view_interface<sentence_input_view<V>> {
  using Unit = detail::unit_of<V>;

 public:
  class iterator {
   public:
    using value_type = std::basic_string_view<Unit>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::input_iterator_tag;

    constexpr explicit iterator(sentence_input_view* view) : view_(view) {}
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
    sentence_input_view* view_;
  };

  constexpr explicit sentence_input_view(V base) : base_(std::move(base)) {}

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
  constexpr void find() {
    if (buffer_.empty() && !pull()) {
      done_ = true;
      return;
    }
    for (;;) {
      const auto first = buffer_.cbegin();
      const auto last = buffer_.cend();
      const auto boundary = detail::sentence_rules::next_boundary<Unit>(first, last);
      if (read_all_ || (boundary != last && detail::sentence_rules::settled<Unit>(boundary, last))) {
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

// text | sentences, or sentences(text): subranges of text read more than
// once, and kept pieces of text read once.
struct sentences_fn : std::ranges::range_adaptor_closure<sentences_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    using View = detail::all_of_t<Range>;
    if constexpr (std::ranges::forward_range<View>)
      return sentence_view<View>(detail::all_of(std::forward<Range>(range)));
    else
      return sentence_input_view<View>(detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr sentences_fn sentences{};

}  // namespace alef

template <class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::sentence_view<V>> =
    std::ranges::enable_borrowed_range<V>;
