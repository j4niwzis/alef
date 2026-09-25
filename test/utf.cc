import std;
import alef;

namespace {

int failures = 0;

void check(bool ok, std::string_view what) {
  if (!ok) {
    ++failures;
    std::println(std::cerr, "FAIL: {}", what);
  }
}

std::u32string read(std::string_view bytes) {
  return bytes | alef::as_utf32 | std::ranges::to<std::u32string>();
}

// Where each code point begins, and what it is, read forwards and read
// backwards: the same, or reading backwards is wrong.
template <class Text>
bool same_both_ways(const Text& text) {
  const auto view = text | alef::as_utf32;
  using piece = std::pair<std::ptrdiff_t, char32_t>;
  std::vector<piece> forwards;
  for (auto at = view.begin(); at != view.end(); ++at)
    forwards.emplace_back(at.base() - text.begin(), *at);
  std::vector<piece> backwards;
  for (auto at = view.end(); at != view.begin();) {
    --at;
    backwards.emplace_back(at.base() - text.begin(), *at);
  }
  std::ranges::reverse(backwards);
  return forwards == backwards;
}

}  // namespace

// All of it at compile time as well.
static_assert(std::ranges::equal(u8"aé\U0001F389" | alef::as_utf32,
                                 std::u32string_view(U"aé\U0001F389")));
static_assert(std::ranges::equal(U"aé\U0001F389" | alef::as_utf8,
                                 std::u8string_view(u8"aé\U0001F389")));
static_assert(std::ranges::equal(u8"a\U0001F389" | alef::as_utf16,
                                 std::u16string_view(u"a\U0001F389")));
static_assert(std::ranges::distance(u8"aé\U0001F389" | alef::as_utf16) == 4);
static_assert(std::ranges::distance("" | alef::as_utf32) == 0);
static_assert(alef::is_well_formed(u8"héllo"));
static_assert(!alef::is_well_formed("\xC0\xAF"));  // an overlong '/'
static_assert(std::ranges::bidirectional_range<decltype(std::string_view() | alef::as_utf32)>);
static_assert(std::ranges::common_range<decltype(std::string_view() | alef::as_utf32)>);
static_assert(*std::ranges::prev((u8"a\U0001F389" | alef::as_utf32).end()) == U'\U0001F389');

int main() {
  // The example in section 3.9 of the Unicode Standard: each maximal subpart
  // of an ill-formed sequence is one U+FFFD.
  check(read("\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64") ==
            U"a���b�c��d",
        "maximal subparts, section 3.9");
  // A surrogate written out is not UTF-8, and no prefix of it is either.
  check(read("\xED\xA0\x80") == U"���", "surrogate");
  // Nor is anything past U+10FFFF.
  check(read("\xF4\x90\x80\x80") == U"����", "past U+10FFFF");
  // A sequence cut short at the end is one replacement.
  check(read("\xE2\x82") == U"�", "cut short");
  // And the bytes that can begin nothing are one each.
  check(read("\xC0\xC1\xF5\xFF") == U"����",
        "bytes that begin nothing");

  // UTF-16: each surrogate without its other half.
  const std::u16string halves{u'a', char16_t(0xD800), u'b', char16_t(0xDC00),
                              char16_t(0xD83C)};
  check((halves | alef::as_utf32 | std::ranges::to<std::u32string>()) ==
            U"a�b��",
        "unpaired surrogates");
  // UTF-32: a surrogate, and a value past U+10FFFF.
  const std::u32string values{U'a', char32_t(0xD800), char32_t(0x110000), U'b'};
  check((values | alef::as_utf8 | std::ranges::to<std::u8string>()) ==
            u8"a��b",
        "UTF-32 that is not");

  // Every scalar value, through all three and back.
  std::u32string scalars;
  for (char32_t one = 0; one <= 0x10FFFF; ++one)
    if (one < 0xD800 || one > 0xDFFF)
      scalars.push_back(one);
  const auto in_utf8 = scalars | alef::as_utf8 | std::ranges::to<std::u8string>();
  const auto in_utf16 = in_utf8 | alef::as_utf16 | std::ranges::to<std::u16string>();
  const auto back = in_utf16 | alef::as_utf32 | std::ranges::to<std::u32string>();
  check(back == scalars, "every scalar value, UTF-32 to 8 to 16 to 32");
  check(alef::is_well_formed(in_utf8) && alef::is_well_formed(in_utf16),
        "what was written is well-formed");

  // From something that can be read only once.
  std::istringstream stream("a\xC3\xA9\xE2\x82\xAC");
  stream >> std::noskipws;
  check((std::views::istream<char>(stream) | alef::as_utf32 |
         std::ranges::to<std::u32string>()) == U"aé€",
        "read once");
  // And a temporary string, which the view keeps.
  check(std::ranges::distance(std::string("a\xC3\xA9") | alef::as_utf32) == 2,
        "a temporary string");

  // Read backwards, text comes apart where it does forwards: every string of
  // two bytes, and a great many longer ones made of the bytes that matter;
  // and UTF-16 made of surrogates and what is next to them.
  for (unsigned first = 0; first < 256; ++first)
    for (unsigned second = 0; second < 256; ++second)
      if (!same_both_ways(std::string{static_cast<char>(first),
                                      static_cast<char>(second)})) {
        check(false, std::format("backwards: {:02X} {:02X}", first, second));
        first = 256;
        break;
      }
  constexpr std::uint8_t bytes[] = {
      0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2,
      0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5,
      0xFF};
  constexpr char16_t units[] = {0x0041, 0xD7FF, 0xD800, 0xDBFF, 0xDC00,
                                0xDFFF, 0xE000, 0xFFFF};
  std::uint64_t state = 0x9E3779B97F4A7C15;
  const auto next = [&] {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::uint32_t>(state >> 33);
  };
  for (int sample = 0; sample < 200000; ++sample) {
    std::string text(1 + next() % 9, '\0');
    for (char& byte : text)
      byte = static_cast<char>(bytes[next() % std::size(bytes)]);
    if (!same_both_ways(text)) {
      check(false, "backwards, UTF-8");
      break;
    }
    std::u16string wide(1 + next() % 6, u'\0');
    for (char16_t& unit : wide)
      unit = units[next() % std::size(units)];
    if (!same_both_ways(wide)) {
      check(false, "backwards, UTF-16");
      break;
    }
  }

  std::println("utf: {}", failures ? "failed" : "ok");
  return failures ? 1 : 0;
}
