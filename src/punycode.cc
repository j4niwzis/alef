// Punycode: RFC 3492, Bootstring for Unicode in domain names -- a string of
// code points as a string of letters, digits and hyphens, and back.
export module alef.punycode;

import std;

namespace alef::detail::punycode {

inline constexpr std::uint32_t base = 36, tmin = 1, tmax = 26, skew = 38, damp = 700;
inline constexpr std::uint32_t initial_bias = 72;
inline constexpr char32_t initial_n = 0x80;
inline constexpr std::uint32_t most = std::numeric_limits<std::uint32_t>::max();

constexpr char digit(std::uint32_t d) {
  return static_cast<char>(d < 26 ? 'a' + d : '0' + (d - 26));
}

constexpr std::optional<std::uint32_t> value_of(char one) {
  if (one >= '0' && one <= '9')
    return static_cast<std::uint32_t>(one - '0' + 26);
  if (one >= 'a' && one <= 'z')
    return static_cast<std::uint32_t>(one - 'a');
  if (one >= 'A' && one <= 'Z')
    return static_cast<std::uint32_t>(one - 'A');
  return std::nullopt;
}

// Section 6.1, bias adaptation.
constexpr std::uint32_t adapt(std::uint32_t delta, std::uint32_t points, bool first) {
  delta = first ? delta / damp : delta / 2;
  delta += delta / points;
  std::uint32_t k = 0;
  while (delta > ((base - tmin) * tmax) / 2) {
    delta /= base - tmin;
    k += base;
  }
  return k + (base - tmin + 1) * delta / (delta + skew);
}

constexpr std::uint32_t threshold(std::uint32_t k, std::uint32_t bias) {
  return k <= bias ? tmin : k >= bias + tmax ? tmax : k - bias;
}

}  // namespace alef::detail::punycode

export namespace alef {

// The Punycode of a string of code points (section 6.3), or nothing where it
// would overflow. Letters it makes are lower case.
constexpr std::optional<std::string> punycode_encode(std::u32string_view input) {
  using namespace detail::punycode;
  std::string out;
  for (const char32_t one : input)
    if (one < 0x80)
      out.push_back(static_cast<char>(one));
  const std::uint32_t basic = static_cast<std::uint32_t>(out.size());
  std::uint32_t handled = basic;
  if (basic > 0)
    out.push_back('-');
  char32_t n = initial_n;
  std::uint32_t delta = 0, bias = initial_bias;
  while (handled < input.size()) {
    char32_t m = 0x110000;
    for (const char32_t one : input)
      if (one >= n && one < m)
        m = one;
    if ((m - n) > (most - delta) / (handled + 1))
      return std::nullopt;
    delta += static_cast<std::uint32_t>(m - n) * (handled + 1);
    n = m;
    for (const char32_t one : input) {
      if (one < n && ++delta == 0)
        return std::nullopt;
      if (one == n) {
        std::uint32_t q = delta;
        for (std::uint32_t k = base;; k += base) {
          const std::uint32_t t = threshold(k, bias);
          if (q < t)
            break;
          out.push_back(digit(t + (q - t) % (base - t)));
          q = (q - t) / (base - t);
        }
        out.push_back(digit(q));
        bias = adapt(delta, handled + 1, handled == basic);
        delta = 0;
        ++handled;
      }
    }
    ++delta;
    ++n;
  }
  return out;
}

// The code points a Punycode string stands for (section 6.2), or nothing
// where it is not one: a letter or digit out of place, an overflow, a code
// point past U+10FFFF or a surrogate. Digits are read in either case.
constexpr std::optional<std::u32string> punycode_decode(std::string_view input) {
  using namespace detail::punycode;
  std::u32string out;
  const std::size_t delimiter = input.rfind('-');
  std::size_t at = 0;
  if (delimiter != std::string_view::npos) {
    for (; at < delimiter; ++at) {
      if (static_cast<unsigned char>(input[at]) >= 0x80)
        return std::nullopt;
      out.push_back(static_cast<char32_t>(input[at]));
    }
    ++at;
  }
  char32_t n = initial_n;
  std::uint32_t i = 0, bias = initial_bias;
  while (at < input.size()) {
    const std::uint32_t old = i;
    std::uint32_t w = 1;
    for (std::uint32_t k = base;; k += base) {
      if (at >= input.size())
        return std::nullopt;
      const std::optional<std::uint32_t> d = value_of(input[at++]);
      if (!d || *d > (most - i) / w)
        return std::nullopt;
      i += *d * w;
      const std::uint32_t t = threshold(k, bias);
      if (*d < t)
        break;
      if (w > most / (base - t))
        return std::nullopt;
      w *= base - t;
    }
    const std::uint32_t points = static_cast<std::uint32_t>(out.size()) + 1;
    bias = adapt(i - old, points, old == 0);
    if (i / points > 0x10FFFF - n)
      return std::nullopt;
    n += i / points;
    i %= points;
    if (n >= 0xD800 && n <= 0xDFFF)
      return std::nullopt;
    out.insert(out.begin() + i, n);
    ++i;
  }
  return out;
}

}  // namespace alef
