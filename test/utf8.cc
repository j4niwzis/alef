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

std::vector<char32_t> read(std::string_view bytes) {
  std::vector<char32_t> out;
  for (const char32_t one : alef::code_points(bytes))
    out.push_back(one);
  return out;
}

// What reading forwards and reading backwards make of some bytes: where each
// code point begins, how long it is, what it is.
struct piece {
  std::size_t offset;
  std::size_t length;
  char32_t code_point;
  bool operator==(const piece&) const = default;
};

bool same_both_ways(std::string_view bytes) {
  std::vector<piece> forwards;
  const auto view = alef::code_points(bytes);
  for (auto at = view.begin(); at != view.end(); ++at)
    forwards.push_back({at.offset(), at.bytes().size(), *at});
  std::vector<piece> backwards;
  for (auto at = view.end(); at != view.begin();) {
    --at;
    backwards.push_back({at.offset(), at.bytes().size(), *at});
  }
  std::ranges::reverse(backwards);
  return forwards == backwards;
}

}  // namespace

// All of it at compile time as well, from what text comes in.
static_assert(alef::decode("A").code_point == U'A');
static_assert(alef::decode(std::string_view{}).length == 0);
static_assert(alef::decode("").code_point == alef::replacement_character);
static_assert(!alef::decode(u8"").well_formed);
static_assert(alef::is_well_formed(u8"héllo, мир, \U0001F389"));
static_assert(!alef::is_well_formed("\xC0\xAF"));  // an overlong '/'
static_assert(alef::encode(U'€') == u8"€");
static_assert(std::ranges::distance(alef::code_points(u8"aé\U0001F389")) == 3);
static_assert(std::ranges::distance(u8"aé\U0001F389" | alef::code_points) == 3);
static_assert(*std::ranges::prev(alef::code_points(u8"aé\U0001F389").end()) == U'\U0001F389');
constexpr std::array<char8_t, 3> bytes_of_a_ha{u8'a', 0xC3, 0xA9};
static_assert(std::ranges::distance(bytes_of_a_ha | alef::code_points) == 2);
static_assert(std::ranges::bidirectional_range<decltype(alef::code_points("x"))>);
static_assert(std::ranges::common_range<decltype(alef::code_points("x"))>);

int main() {
  // The example in section 3.9 of the Unicode Standard: each maximal subpart
  // of an ill-formed sequence is one U+FFFD.
  check(read("\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64") ==
            std::vector<char32_t>{U'a', 0xFFFD, 0xFFFD, 0xFFFD, U'b', 0xFFFD,
                                  U'c', 0xFFFD, 0xFFFD, U'd'},
        "maximal subparts, section 3.9");
  // A surrogate written out is not UTF-8, and no prefix of it is either.
  check(read("\xED\xA0\x80") == std::vector<char32_t>{0xFFFD, 0xFFFD, 0xFFFD},
        "surrogate");
  // Nor is anything past U+10FFFF.
  check(read("\xF4\x90\x80\x80") ==
            std::vector<char32_t>{0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD},
        "past U+10FFFF");
  // A sequence cut short at the end is one replacement.
  check(read("\xE2\x82") == std::vector<char32_t>{0xFFFD}, "cut short");
  // And the bytes that can begin nothing are one each.
  check(read("\xC0\xC1\xF5\xFF") ==
            std::vector<char32_t>{0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD},
        "bytes that begin nothing");

  // Whatever holds the bytes.
  const std::vector<char8_t> vector{u8'a', 0xC3, 0xA9};
  check(std::ranges::distance(vector | alef::code_points) == 2, "a vector");
  const std::string string = "a\xC3\xA9";
  check(std::ranges::distance(std::span(string) | alef::code_points) == 2,
        "a span");

  // Every scalar value, written and read back.
  for (char32_t one = 0; one <= 0x10FFFF; ++one) {
    if (one >= 0xD800 && one <= 0xDFFF)
      continue;
    const alef::encoded bytes = alef::encode(one);
    const alef::decoded back = alef::decode(bytes.view());
    if (!back.well_formed || back.code_point != one ||
        back.length != bytes.length) {
      check(false, std::format("U+{:04X} round trip", std::uint32_t(one)));
      break;
    }
  }
  check(alef::encode(0xD800) == u8"�", "a surrogate is written as U+FFFD");

  // Read backwards, text comes apart where it does forwards: every string of
  // two bytes, and a great many longer ones made of the bytes that matter.
  for (unsigned first = 0; first < 256; ++first)
    for (unsigned second = 0; second < 256; ++second) {
      const char bytes[] = {static_cast<char>(first), static_cast<char>(second)};
      if (!same_both_ways(std::string_view(bytes, 2))) {
        check(false, std::format("backwards: {:02X} {:02X}", first, second));
        first = 256;
        break;
      }
    }
  constexpr std::uint8_t interesting[] = {
      0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2,
      0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5,
      0xFF};
  std::uint64_t state = 0x9E3779B97F4A7C15;
  const auto next = [&] {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::uint32_t>(state >> 33);
  };
  for (int sample = 0; sample < 200000; ++sample) {
    std::string bytes(1 + next() % 9, '\0');
    for (char& byte : bytes)
      byte = static_cast<char>(interesting[next() % std::size(interesting)]);
    if (!same_both_ways(bytes)) {
      std::string shown;
      for (const char byte : bytes)
        shown += std::format("{:02X} ", static_cast<std::uint8_t>(byte));
      check(false, "backwards: " + shown);
      break;
    }
  }

  std::println("utf8: {}", failures ? "failed" : "ok");
  return failures ? 1 : 0;
}
