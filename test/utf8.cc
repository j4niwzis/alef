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

}  // namespace

// All of it at compile time as well.
static_assert(alef::decode(std::string_view("A")).code_point == U'A');
static_assert(alef::is_well_formed(std::u8string_view(u8"héllo, мир, \U0001F389")));
static_assert(!alef::is_well_formed(std::string_view("\xC0\xAF")));  // overlong '/'
static_assert(alef::encode(U'€').view() == u8"€");
static_assert(std::ranges::distance(alef::code_points(std::u8string_view(u8"aé\U0001F389"))) == 3);

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
  check(alef::encode(0xD800).view() == u8"�", "a surrogate is written as U+FFFD");

  std::println("utf8: {}", failures ? "failed" : "ok");
  return failures ? 1 : 0;
}
