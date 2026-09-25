// The UTFs: UTF-8, UTF-16 and UTF-32, read lazily and written lazily.
//
// Which of them some text is in, the type of its code units says: char and
// char8_t are UTF-8, char16_t is UTF-16, char32_t is UTF-32. as_utf8,
// as_utf16 and as_utf32 read any of them and give the same text in another,
// one code unit at a time, as they are asked for -- over a string, a view, a
// file read once, whatever range the code units come from. A code point is a
// char32_t on its way through, and nothing is written anywhere to be looked
// at later.
//
// What is not well-formed is read as U+FFFD: in UTF-8, one for each maximal
// subpart of an ill-formed sequence, the practice the Unicode Standard
// recommends (section 3.9) and the WHATWG encoding standard requires; in
// UTF-16, one for each unpaired surrogate; in UTF-32, one for each value that
// is a surrogate or past U+10FFFF. Read backwards, text comes apart at the
// same places it does forwards.
export module alef:utf;

import std;

export namespace alef {

// U+FFFD REPLACEMENT CHARACTER.
inline constexpr char32_t replacement_character = U'�';

template <class Unit>
concept utf8_code_unit = std::same_as<Unit, char> || std::same_as<Unit, char8_t>;
template <class Unit>
concept utf16_code_unit = std::same_as<Unit, char16_t>;
template <class Unit>
concept utf32_code_unit = std::same_as<Unit, char32_t>;
template <class Unit>
concept code_unit =
    utf8_code_unit<Unit> || utf16_code_unit<Unit> || utf32_code_unit<Unit>;

// A range of the code units of one of the UTFs.
template <class Range>
concept utf_range =
    std::ranges::input_range<Range> &&
    code_unit<std::remove_cv_t<std::ranges::range_value_t<Range>>>;

}  // namespace alef

namespace alef::detail {

template <class Range>
using unit_of = std::remove_cv_t<std::ranges::range_value_t<Range>>;

// What an iterator does not keep when its range cannot give it.
struct nothing {};

constexpr bool continuation(std::uint8_t byte) noexcept {
  return byte >= 0x80 && byte <= 0xBF;
}

// The code point `at` is at, which is not `last`; `at` is moved past what was
// read -- the code point, or what is replaced by one U+FFFD -- and only past
// that: a code unit that ends an ill-formed sequence without belonging to it
// is looked at and not taken, so a range that can be read once reads it as
// the start of what comes next. `taken` is given each code unit that is
// taken, as it is: all that is left of it, in text read once.
template <code_unit Unit, class I, class S, class Taken>
constexpr char32_t read(I& at, const S& last, bool& well_formed, Taken&& taken) {
  if constexpr (utf8_code_unit<Unit>) {
    const Unit first = *at;
    const auto lead = static_cast<std::uint8_t>(first);
    taken(first);
    ++at;
    if (lead < 0x80) {
      well_formed = true;
      return lead;
    }
    // The well-formed byte sequences (the Unicode Standard, table 3-7): the
    // lead byte says how long the sequence is and narrows what its second
    // byte may be, which keeps out overlong forms, surrogates and anything
    // past U+10FFFF.
    int length = 0;
    char32_t value = 0;
    std::uint8_t low = 0x80;
    std::uint8_t high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
      value = lead & 0x1F;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
      value = lead & 0x0F;
      if (lead == 0xE0)
        low = 0xA0;
      else if (lead == 0xED)
        high = 0x9F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
      value = lead & 0x07;
      if (lead == 0xF0)
        low = 0x90;
      else if (lead == 0xF4)
        high = 0x8F;
    } else {
      well_formed = false;
      return replacement_character;
    }
    for (int done = 1; done < length; ++done) {
      if (at == last) {
        well_formed = false;
        return replacement_character;
      }
      const Unit unit = *at;
      const auto next = static_cast<std::uint8_t>(unit);
      if (next < low || next > high) {
        well_formed = false;
        return replacement_character;
      }
      taken(unit);
      ++at;
      value = (value << 6) | (next & 0x3F);
      low = 0x80;
      high = 0xBF;
    }
    well_formed = true;
    return value;
  } else if constexpr (utf16_code_unit<Unit>) {
    const char16_t first = *at;
    taken(first);
    ++at;
    if (first < 0xD800 || first > 0xDFFF) {
      well_formed = true;
      return first;
    }
    if (first >= 0xDC00 || at == last) {
      well_formed = false;
      return replacement_character;
    }
    const char16_t second = *at;
    if (second < 0xDC00 || second > 0xDFFF) {
      well_formed = false;
      return replacement_character;
    }
    taken(second);
    ++at;
    well_formed = true;
    return 0x10000 + ((char32_t(first) - 0xD800) << 10) +
           (char32_t(second) - 0xDC00);
  } else {
    const char32_t value = *at;
    taken(value);
    ++at;
    well_formed = !((value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF);
    return well_formed ? value : replacement_character;
  }
}

template <code_unit Unit, class I, class S>
constexpr char32_t read(I& at, const S& last, bool& well_formed) {
  return read<Unit>(at, last, well_formed, [](Unit) {});
}

// Where the code point that ends at `at` begins. `at` is not `first`, and is
// where a code point begins, or the end.
template <code_unit Unit, class I>
constexpr I step_back(const I& first, I at) {
  if constexpr (utf8_code_unit<Unit>) {
    // A lead byte always begins a code point when text is read forwards, and
    // what it begins is at most four bytes long. So the code point before
    // `at` begins at the nearest lead byte within four back, if what that
    // byte begins ends exactly at `at`; otherwise the byte before `at` is a
    // continuation byte of nothing, and is one on its own.
    I lead = at;
    int back = 0;
    do {
      --lead;
      ++back;
    } while (lead != first && back < 4 &&
             continuation(static_cast<std::uint8_t>(*lead)));
    if (!continuation(static_cast<std::uint8_t>(*lead))) {
      I probe = lead;
      bool well_formed = false;
      read<Unit>(probe, at, well_formed);
      if (probe == at)
        return lead;
    }
    return std::ranges::prev(at);
  } else if constexpr (utf16_code_unit<Unit>) {
    I previous = std::ranges::prev(at);
    const char16_t unit = *previous;
    if (unit >= 0xDC00 && unit <= 0xDFFF && previous != first) {
      I before = std::ranges::prev(previous);
      const char16_t high = *before;
      if (high >= 0xD800 && high <= 0xDBFF)
        return before;
    }
    return previous;
  } else {
    return std::ranges::prev(at);
  }
}

// Whether a code point begins at `at`, which is not `last`.
template <code_unit Unit, class I, class S>
constexpr bool starts(const I& first, const I& at, const S& last) {
  if constexpr (utf8_code_unit<Unit>) {
    if (!continuation(static_cast<std::uint8_t>(*at)))
      return true;
    I lead = at;
    for (int back = 0; lead != first && back < 3;) {
      --lead;
      ++back;
      if (!continuation(static_cast<std::uint8_t>(*lead))) {
        I probe = lead;
        bool well_formed = false;
        read<Unit>(probe, last, well_formed);
        return std::ranges::distance(lead, probe) <= back;
      }
    }
    return true;
  } else if constexpr (utf16_code_unit<Unit>) {
    const char16_t unit = *at;
    if (unit < 0xDC00 || unit > 0xDFFF || at == first)
      return true;
    const char16_t high = *std::ranges::prev(at);
    return !(high >= 0xD800 && high <= 0xDBFF);
  } else {
    return true;
  }
}

// `code_point`, a scalar value, as code units of the UTF `Unit` is of; how
// many.
template <code_unit Unit>
constexpr std::uint8_t write(char32_t code_point,
                             std::array<Unit, 4>& out) noexcept {
  const auto put = [&](int at, char32_t bits) {
    out[at] = static_cast<Unit>(bits);
  };
  if constexpr (utf8_code_unit<Unit>) {
    if (code_point < 0x80) {
      put(0, code_point);
      return 1;
    }
    if (code_point < 0x800) {
      put(0, 0xC0 | (code_point >> 6));
      put(1, 0x80 | (code_point & 0x3F));
      return 2;
    }
    if (code_point < 0x10000) {
      put(0, 0xE0 | (code_point >> 12));
      put(1, 0x80 | ((code_point >> 6) & 0x3F));
      put(2, 0x80 | (code_point & 0x3F));
      return 3;
    }
    put(0, 0xF0 | (code_point >> 18));
    put(1, 0x80 | ((code_point >> 12) & 0x3F));
    put(2, 0x80 | ((code_point >> 6) & 0x3F));
    put(3, 0x80 | (code_point & 0x3F));
    return 4;
  } else if constexpr (utf16_code_unit<Unit>) {
    if (code_point < 0x10000) {
      put(0, code_point);
      return 1;
    }
    code_point -= 0x10000;
    put(0, 0xD800 + (code_point >> 10));
    put(1, 0xDC00 + (code_point & 0x3FF));
    return 2;
  } else {
    put(0, code_point);
    return 1;
  }
}

// A range as the views here take it: all of it, except that an array of code
// units -- a string literal -- stops before the NUL it ends in, which is not
// part of the text.
template <class Range>
constexpr auto all_of(Range&& range) {
  using Plain = std::remove_reference_t<Range>;
  if constexpr (std::is_array_v<Plain>) {
    constexpr std::size_t size = std::extent_v<Plain>;
    const std::size_t used = size > 0 && range[size - 1] == 0 ? size - 1 : size;
    return std::ranges::subrange(std::ranges::begin(range),
                                 std::ranges::begin(range) + used);
  } else {
    return std::views::all(std::forward<Range>(range));
  }
}

template <class Range>
using all_of_t = decltype(all_of(std::declval<Range>()));

}  // namespace alef::detail

export namespace alef {

// The code units of V, read as code points and written as code units of the
// UTF `To` is of. As lazy as V: an input range if V can be read only once, a
// forward range if V is one, a bidirectional range if V is one.
template <code_unit To, std::ranges::view V>
  requires utf_range<V>
class utf_view : public std::ranges::view_interface<utf_view<To, V>> {
  using From = detail::unit_of<V>;

  template <bool Const>
  class iterator {
    using Base = std::conditional_t<Const, const V, V>;
    using I = std::ranges::iterator_t<Base>;
    using S = std::ranges::sentinel_t<Base>;
    static constexpr bool forward = std::ranges::forward_range<Base>;
    static constexpr bool bidirectional = std::ranges::bidirectional_range<Base>;

   public:
    using value_type = To;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::conditional_t<
        bidirectional, std::bidirectional_iterator_tag,
        std::conditional_t<forward, std::forward_iterator_tag,
                           std::input_iterator_tag>>;

    iterator()
      requires std::default_initializable<I>
    = default;

    constexpr iterator(Base& base, I at)
        : next_(std::move(at)), last_(std::ranges::end(base)) {
      if constexpr (bidirectional)
        first_ = std::ranges::begin(base);
      read();
    }

    constexpr To operator*() const { return units_[index_]; }

    constexpr iterator& operator++() {
      if (++index_ >= count_)
        read();
      return *this;
    }
    constexpr void operator++(int)
      requires(!forward)
    {
      ++*this;
    }
    constexpr iterator operator++(int)
      requires forward
    {
      iterator was = *this;
      ++*this;
      return was;
    }
    constexpr iterator& operator--()
      requires bidirectional
    {
      if (index_ > 0) {
        --index_;
        return *this;
      }
      next_ = detail::step_back<From>(first_, current_);
      read();
      index_ = count_ - 1;
      return *this;
    }
    constexpr iterator operator--(int)
      requires bidirectional
    {
      iterator was = *this;
      --*this;
      return was;
    }

    friend constexpr bool operator==(const iterator& one, const iterator& other)
      requires forward
    {
      return one.current_ == other.current_ && one.index_ == other.index_;
    }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) {
      return one.count_ == 0;
    }

    // Where the code point being written begins in V.
    constexpr I base() const
      requires forward
    {
      return current_;
    }
    // Whether it was read from a well-formed sequence, rather than replaced.
    constexpr bool well_formed() const noexcept { return well_formed_; }

   private:
    constexpr void read() {
      if constexpr (forward)
        current_ = next_;
      index_ = 0;
      if (next_ == last_) {
        count_ = 0;
        return;
      }
      bool well_formed = false;
      const char32_t code_point = detail::read<From>(next_, last_, well_formed);
      well_formed_ = well_formed;
      count_ = detail::write<To>(code_point, units_);
    }

    [[no_unique_address]] std::conditional_t<bidirectional, I, detail::nothing>
        first_{};
    [[no_unique_address]] std::conditional_t<forward, I, detail::nothing>
        current_{};
    I next_{};
    [[no_unique_address]] S last_{};
    std::array<To, 4> units_{};
    std::uint8_t count_ = 0;
    std::uint8_t index_ = 0;
    bool well_formed_ = true;
  };

 public:
  utf_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit utf_view(V base) : base_(std::move(base)) {}

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
    requires utf_range<const V>
  {
    return iterator<true>(base_, std::ranges::begin(base_));
  }
  constexpr auto end() {
    if constexpr (std::ranges::forward_range<V> && std::ranges::common_range<V>)
      return iterator<false>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }
  constexpr auto end() const
    requires utf_range<const V>
  {
    if constexpr (std::ranges::forward_range<const V> &&
                  std::ranges::common_range<const V>)
      return iterator<true>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }

 private:
  V base_ = V();
};

// text | as_utf8, text | as_utf16, text | as_utf32: text of any UTF, in
// the one named.
template <code_unit To>
struct as_utf_fn : std::ranges::range_adaptor_closure<as_utf_fn<To>> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    return utf_view<To, detail::all_of_t<Range>>(
        detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr as_utf_fn<char8_t> as_utf8{};
inline constexpr as_utf_fn<char16_t> as_utf16{};
inline constexpr as_utf_fn<char32_t> as_utf32{};

// Whether all of `text` is well-formed. Reads it once, and no further than
// the first thing that is not.
template <std::ranges::viewable_range Range>
  requires utf_range<Range>
constexpr bool is_well_formed(Range&& range) {
  auto text = detail::all_of(std::forward<Range>(range));
  auto at = std::ranges::begin(text);
  const auto last = std::ranges::end(text);
  while (at != last) {
    bool well_formed = false;
    detail::read<detail::unit_of<decltype(text)>>(at, last, well_formed);
    if (!well_formed)
      return false;
  }
  return true;
}

}  // namespace alef

template <class To, class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::utf_view<To, V>> =
    std::ranges::enable_borrowed_range<V>;
