// UTF-8, and the code points read from it.
//
// Text is kept as UTF-8. It is read one code point at a time, and nothing
// here turns it into another encoding in memory in order to look at it: a
// code point is a char32_t on its way through an algorithm, not a format.
//
// Bytes that are not UTF-8 are read as U+FFFD, one for each maximal subpart
// of an ill-formed sequence. That is the practice the Unicode Standard
// recommends (section 3.9, "U+FFFD Substitution of Maximal Subparts") and the
// one the WHATWG encoding standard requires: reading resumes at the first
// byte that could begin a character, and two readers agree on how many
// replacements a stretch of bad bytes is.
export module alef:utf8;

import std;

export namespace alef {

// U+FFFD REPLACEMENT CHARACTER.
inline constexpr char32_t replacement_character = U'�';

// What UTF-8 is held in: char, which most interfaces hand over, or char8_t,
// which says what it holds.
template <class Char>
concept utf8_char = std::same_as<Char, char> || std::same_as<Char, char8_t>;

// One code point read from the front of some UTF-8.
struct decoded {
  // The code point, or U+FFFD where the bytes are not UTF-8.
  char32_t code_point = 0;
  // How many bytes were read: the sequence, or the maximal subpart of an
  // ill-formed one, which is replaced as a unit.
  std::uint8_t length = 0;
  bool well_formed = false;
};

// The code point at the front of `text`, which is not empty.
template <utf8_char Char>
constexpr decoded decode(std::basic_string_view<Char> text) noexcept {
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

constexpr decoded decode(std::string_view text) noexcept {
  return alef::decode<char>(text);
}
constexpr decoded decode(std::u8string_view text) noexcept {
  return alef::decode<char8_t>(text);
}

// Whether all of `text` is UTF-8.
template <utf8_char Char>
constexpr bool is_well_formed(std::basic_string_view<Char> text) noexcept {
  while (!text.empty()) {
    const decoded one = alef::decode<Char>(text);
    if (!one.well_formed)
      return false;
    text.remove_prefix(one.length);
  }
  return true;
}

constexpr bool is_well_formed(std::string_view text) noexcept {
  return alef::is_well_formed<char>(text);
}
constexpr bool is_well_formed(std::u8string_view text) noexcept {
  return alef::is_well_formed<char8_t>(text);
}

// A code point written as UTF-8.
struct encoded {
  std::array<char8_t, 4> bytes{};
  std::uint8_t length = 0;

  constexpr std::u8string_view view() const noexcept {
    return {bytes.data(), length};
  }
};

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

// The code points of some UTF-8, read as they are asked for.
template <utf8_char Char>
class code_point_view
    : public std::ranges::view_interface<code_point_view<Char>> {
 public:
  class iterator {
   public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

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
      read_ = at_ < text_.size() ? alef::decode<Char>(text_.substr(at_))
                                 : decoded{};
    }

    std::basic_string_view<Char> text_;
    std::size_t at_ = 0;
    decoded read_;
  };

  constexpr code_point_view() = default;
  constexpr explicit code_point_view(std::basic_string_view<Char> text) noexcept
      : text_(text) {}

  constexpr iterator begin() const noexcept { return iterator(text_, 0); }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  std::basic_string_view<Char> text_;
};

constexpr code_point_view<char> code_points(std::string_view text) noexcept {
  return code_point_view<char>(text);
}
constexpr code_point_view<char8_t> code_points(
    std::u8string_view text) noexcept {
  return code_point_view<char8_t>(text);
}

}  // namespace alef

// It holds a view of the text, not the text.
template <class Char>
inline constexpr bool
    std::ranges::enable_borrowed_range<alef::code_point_view<Char>> = true;
