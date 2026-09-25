// UTF-8, and the code points read from it.
//
// Text is kept as UTF-8. It is read one code point at a time, forwards or
// backwards, and nothing here turns it into another encoding in memory in
// order to look at it: a code point is a char32_t on its way through an
// algorithm, not a format.
//
// Bytes that are not UTF-8 are read as U+FFFD, one for each maximal subpart
// of an ill-formed sequence. That is the practice the Unicode Standard
// recommends (section 3.9, "U+FFFD Substitution of Maximal Subparts") and the
// one the WHATWG encoding standard requires: reading resumes at the first
// byte that could begin a character, and two readers agree on how many
// replacements a stretch of bad bytes is. Read backwards, the same text comes
// apart at the same places.
export module alef:utf8;

import std;

export namespace alef {

// U+FFFD REPLACEMENT CHARACTER.
inline constexpr char32_t replacement_character = U'�';

// What UTF-8 is held in: char, which most interfaces hand over, or char8_t,
// which says what it holds.
template <class Char>
concept utf8_char = std::same_as<Char, char> || std::same_as<Char, char8_t>;

// Something UTF-8 can be read from: a string or a view of one, a string
// literal, a pointer to a NUL-terminated string, or any contiguous range of
// char or char8_t -- a vector of bytes, a span.
template <class Text>
concept utf8_text =
    std::convertible_to<Text, std::string_view> ||
    std::convertible_to<Text, std::u8string_view> ||
    (std::ranges::contiguous_range<Text> && std::ranges::sized_range<Text> &&
     utf8_char<std::remove_cv_t<std::ranges::range_value_t<Text>>>);

// The bytes of `text`, as a view of the same kind of char. A string literal
// is taken up to its NUL, as std::string_view takes it.
template <utf8_text Text>
constexpr auto as_utf8(Text&& text) noexcept {
  if constexpr (std::convertible_to<Text, std::string_view>) {
    return std::string_view(text);
  } else if constexpr (std::convertible_to<Text, std::u8string_view>) {
    return std::u8string_view(text);
  } else {
    using Char = std::remove_cv_t<std::ranges::range_value_t<Text>>;
    return std::basic_string_view<Char>(std::ranges::data(text),
                                        std::ranges::size(text));
  }
}

// One code point read from the front of some UTF-8.
struct decoded {
  // The code point, or U+FFFD where the bytes are not UTF-8 -- and where
  // there are no bytes.
  char32_t code_point = replacement_character;
  // How many bytes were read: the sequence, or the maximal subpart of an
  // ill-formed one, which is replaced as a unit. None from empty text.
  std::uint8_t length = 0;
  bool well_formed = false;
};

// A code point written as UTF-8.
struct encoded {
  std::array<char8_t, 4> bytes{};
  std::uint8_t length = 0;

  // A view of the bytes above -- of this object, so not of a temporary one,
  // whose bytes are gone by the time the view is looked at.
  constexpr std::u8string_view view() const& noexcept {
    return {bytes.data(), length};
  }
  std::u8string_view view() const&& = delete;

  friend constexpr bool operator==(const encoded& one,
                                   std::u8string_view other) noexcept {
    return one.view() == other;
  }
};

}  // namespace alef

namespace alef::detail {

template <utf8_char Char>
constexpr decoded decode_front(std::basic_string_view<Char> text) noexcept {
  if (text.empty())
    return {};
  const auto byte = [&](std::size_t at) {
    return static_cast<std::uint8_t>(text[at]);
  };
  const std::uint8_t lead = byte(0);
  if (lead < 0x80)
    return {lead, 1, true};
  // The well-formed byte sequences (the Unicode Standard, table 3-7): the
  // lead byte says how long the sequence is and narrows what its second byte
  // may be, which is what keeps out overlong forms, surrogates and anything
  // past U+10FFFF.
  std::uint8_t length = 0;
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
    return {replacement_character, 1, false};
  }
  for (std::uint8_t at = 1; at < length; ++at) {
    if (at >= text.size())
      return {replacement_character, at, false};
    const std::uint8_t next = byte(at);
    if (next < low || next > high)
      return {replacement_character, at, false};
    value = (value << 6) | (next & 0x3F);
    low = 0x80;
    high = 0xBF;
  }
  return {value, length, true};
}

// Where the code point that ends at `at` begins. `at` is past the start and
// is where a code point begins, or the end.
//
// A lead byte always begins a code point when the text is read forwards, and
// what it begins is at most four bytes long. So the code point before `at`
// begins at the nearest lead byte within four bytes back, if what that byte
// begins ends exactly at `at`; otherwise the byte before `at` is a
// continuation byte that belongs to nothing, and is one on its own.
template <utf8_char Char>
constexpr std::size_t previous_start(std::basic_string_view<Char> text,
                                     std::size_t at) noexcept {
  const auto continuation = [&](std::size_t where) {
    const auto byte = static_cast<std::uint8_t>(text[where]);
    return byte >= 0x80 && byte <= 0xBF;
  };
  const std::size_t floor = at >= 4 ? at - 4 : 0;
  std::size_t lead = at - 1;
  while (lead > floor && continuation(lead))
    --lead;
  if (!continuation(lead) &&
      lead + decode_front(text.substr(lead)).length == at)
    return lead;
  return at - 1;
}

// Whether a code point begins at `at`, which is inside the text.
template <utf8_char Char>
constexpr bool code_point_starts(std::basic_string_view<Char> text,
                                 std::size_t at) noexcept {
  const auto continuation = [&](std::size_t where) {
    const auto byte = static_cast<std::uint8_t>(text[where]);
    return byte >= 0x80 && byte <= 0xBF;
  };
  if (!continuation(at))
    return true;
  const std::size_t floor = at >= 3 ? at - 3 : 0;
  for (std::size_t lead = at; lead > floor;) {
    --lead;
    if (!continuation(lead))
      return lead + decode_front(text.substr(lead)).length <= at;
  }
  return true;
}

// Neither an lvalue nor a view of something that outlives it: a temporary
// string, whose bytes a view would outlive.
template <class Text>
concept lasting = std::is_lvalue_reference_v<Text> ||
                  std::ranges::borrowed_range<Text> ||
                  std::is_pointer_v<std::remove_cvref_t<Text>>;

}  // namespace alef::detail

export namespace alef {

// The code point at the front of `text`.
template <utf8_text Text>
constexpr decoded decode(Text&& text) noexcept {
  return detail::decode_front(alef::as_utf8(text));
}

// Whether all of `text` is UTF-8.
template <utf8_text Text>
constexpr bool is_well_formed(Text&& text) noexcept {
  auto bytes = alef::as_utf8(text);
  while (!bytes.empty()) {
    const decoded one = detail::decode_front(bytes);
    if (!one.well_formed)
      return false;
    bytes.remove_prefix(one.length);
  }
  return true;
}

// `code_point` as UTF-8. A surrogate, or a value past U+10FFFF, is not a
// character of any text, and is written as U+FFFD.
constexpr encoded encode(char32_t code_point) noexcept {
  if ((code_point >= 0xD800 && code_point <= 0xDFFF) || code_point > 0x10FFFF)
    code_point = replacement_character;
  encoded out;
  const auto put = [&](std::uint8_t at, char32_t bits) {
    out.bytes[at] = static_cast<char8_t>(bits);
  };
  if (code_point < 0x80) {
    put(0, code_point);
    out.length = 1;
  } else if (code_point < 0x800) {
    put(0, 0xC0 | (code_point >> 6));
    put(1, 0x80 | (code_point & 0x3F));
    out.length = 2;
  } else if (code_point < 0x10000) {
    put(0, 0xE0 | (code_point >> 12));
    put(1, 0x80 | ((code_point >> 6) & 0x3F));
    put(2, 0x80 | (code_point & 0x3F));
    out.length = 3;
  } else {
    put(0, 0xF0 | (code_point >> 18));
    put(1, 0x80 | ((code_point >> 12) & 0x3F));
    put(2, 0x80 | ((code_point >> 6) & 0x3F));
    put(3, 0x80 | (code_point & 0x3F));
    out.length = 4;
  }
  return out;
}

// The code points of some UTF-8, read as they are asked for, in either
// direction.
template <utf8_char Char>
class code_point_view
    : public std::ranges::view_interface<code_point_view<Char>> {
 public:
  class iterator {
   public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::bidirectional_iterator_tag;

    constexpr iterator() = default;

    constexpr char32_t operator*() const noexcept { return read_.code_point; }

    constexpr iterator& operator++() noexcept {
      at_ += read_.length;
      read();
      return *this;
    }
    constexpr iterator operator++(int) noexcept {
      iterator was = *this;
      ++*this;
      return was;
    }
    constexpr iterator& operator--() noexcept {
      at_ = detail::previous_start(text_, at_);
      read();
      return *this;
    }
    constexpr iterator operator--(int) noexcept {
      iterator was = *this;
      --*this;
      return was;
    }

    constexpr bool operator==(const iterator& other) const noexcept {
      return at_ == other.at_;
    }
    constexpr bool operator==(std::default_sentinel_t) const noexcept {
      return at_ == text_.size();
    }

    // Where the code point begins in the text, in bytes.
    constexpr std::size_t offset() const noexcept { return at_; }
    // The bytes it was read from.
    constexpr std::basic_string_view<Char> bytes() const noexcept {
      return text_.substr(at_, read_.length);
    }
    // Whether they were UTF-8, rather than replaced.
    constexpr bool well_formed() const noexcept { return read_.well_formed; }

   private:
    friend code_point_view;

    constexpr iterator(std::basic_string_view<Char> text,
                       std::size_t at) noexcept
        : text_(text), at_(at) {
      read();
    }

    constexpr void read() noexcept {
      read_ = detail::decode_front(text_.substr(at_));
    }

    std::basic_string_view<Char> text_;
    std::size_t at_ = 0;
    decoded read_;
  };

  constexpr code_point_view() = default;
  constexpr explicit code_point_view(std::basic_string_view<Char> text) noexcept
      : text_(text) {}

  constexpr iterator begin() const noexcept { return iterator(text_, 0); }
  constexpr iterator end() const noexcept {
    return iterator(text_, text_.size());
  }

 private:
  std::basic_string_view<Char> text_;
};

// code_points(text), or text | code_points.
struct code_points_fn : std::ranges::range_adaptor_closure<code_points_fn> {
  template <utf8_text Text>
    requires detail::lasting<Text>
  constexpr auto operator()(Text&& text) const noexcept {
    const auto bytes = alef::as_utf8(text);
    return code_point_view<typename decltype(bytes)::value_type>(bytes);
  }
};
inline constexpr code_points_fn code_points{};

}  // namespace alef

// It holds a view of the text, not the text.
template <class Char>
inline constexpr bool
    std::ranges::enable_borrowed_range<alef::code_point_view<Char>> = true;
