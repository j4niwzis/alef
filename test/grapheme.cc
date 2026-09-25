import std;
import alef;

// A few at compile time, in all three UTFs.
static_assert(std::ranges::distance(u8"é" | alef::graphemes) == 1);
static_assert(std::ranges::distance(u"é" | alef::graphemes) == 1);
static_assert(std::ranges::distance(U"é" | alef::graphemes) == 1);
static_assert(std::ranges::distance("\r\n" | alef::graphemes) == 1);
// Regional indicators go in pairs: four of them are two clusters.
static_assert(std::ranges::distance(u8"\U0001F1E6\U0001F1E7\U0001F1E6\U0001F1E7" | alef::graphemes) == 2);
// A family: pictographs joined by ZWJ.
static_assert(std::ranges::distance(u8"\U0001F468‍\U0001F469‍\U0001F467" | alef::graphemes) == 1);
// ksha: consonant, virama, consonant (GB9c).
static_assert(std::ranges::distance(u8"क्ष" | alef::graphemes) == 1);
// The example in the README.
static_assert(std::ranges::distance(u8"é\U0001F9D1\u200D\U0001F4BBक्ष" | alef::graphemes) == 3);
static_assert(std::ranges::distance(u8"aé" | alef::graphemes | std::views::reverse) == 2);
static_assert(alef::grapheme_cluster_break_of(U'́') == alef::grapheme_cluster_break::extend);
static_assert(alef::indic_conjunct_break_of(U'्') == alef::indic_conjunct_break::linker);
static_assert(alef::is_extended_pictographic(U'\U0001F389'));
constexpr std::u8string_view accented = u8"aé";
static_assert(alef::next_grapheme_boundary(accented.begin(), accented.end()) == accented.begin() + 1);
static_assert(alef::prev_grapheme_boundary(accented.begin(), accented.end()) == accented.begin() + 1);
static_assert(alef::is_grapheme_boundary(accented.begin(), accented.begin() + 1, accented.end()));
static_assert(!alef::is_grapheme_boundary(accented.begin(), accented.begin() + 2, accented.end()));
static_assert(!alef::is_grapheme_boundary(accented.begin(), accented.begin() + 3, accented.end()));  // inside U+0301
// A temporary string is kept by the view made of it.
static_assert(std::invocable<decltype(alef::graphemes), std::string>);

namespace {

int failures = 0;

void fail(std::string_view what) {
  if (++failures <= 20)
    std::println(std::cerr, "FAIL: {}", what);
}

std::vector<std::size_t> forwards(std::string_view text) {
  std::vector<std::size_t> found{0};
  for (const auto cluster : text | alef::graphemes)
    found.push_back(static_cast<std::size_t>(cluster.end() - text.begin()));
  return found;
}

std::vector<std::size_t> backwards(std::string_view text) {
  std::vector<std::size_t> found;
  auto at = text.end();
  found.push_back(text.size());
  while (at != text.begin()) {
    at = alef::prev_grapheme_boundary(text.begin(), at);
    found.push_back(static_cast<std::size_t>(at - text.begin()));
  }
  std::ranges::reverse(found);
  if (text.empty())
    found = {0};
  return found;
}

std::vector<std::size_t> everywhere(std::string_view text) {
  std::vector<std::size_t> found;
  for (auto at = text.begin();; ++at) {
    if (alef::is_grapheme_boundary(text.begin(), at, text.end()))
      found.push_back(static_cast<std::size_t>(at - text.begin()));
    if (at == text.end())
      break;
  }
  return found;
}

std::vector<std::string_view> clusters(std::string_view text) {
  std::vector<std::string_view> out;
  for (const auto cluster : text | alef::graphemes)
    out.emplace_back(cluster.begin(), cluster.end());
  return out;
}

std::vector<std::string_view> reversed(std::string_view text) {
  std::vector<std::string_view> out;
  for (const auto cluster : text | alef::graphemes | std::views::reverse)
    out.emplace_back(cluster.begin(), cluster.end());
  std::ranges::reverse(out);
  return out;
}

// How many code points each cluster of some text is, in whatever UTF.
template <class Text>
std::vector<std::ptrdiff_t> sizes(const Text& text) {
  std::vector<std::ptrdiff_t> out;
  for (const auto cluster : text | alef::graphemes)
    out.push_back(std::ranges::distance(cluster | alef::as_utf32));
  return out;
}

// Every way of finding the boundaries of `text` finds the same ones, and the
// same text in UTF-16 and UTF-32 comes apart into the same clusters.
bool agree(std::string_view text, std::string_view what) {
  const std::vector<std::size_t> expected = forwards(text);
  bool ok = true;
  const auto wrong = [&](std::string_view how) {
    fail(std::format("{}: {}", what, how));
    ok = false;
  };
  if (backwards(text) != expected)
    wrong("backwards");
  if (everywhere(text) != expected)
    wrong("is_grapheme_boundary");
  if (reversed(text) != clusters(text))
    wrong("reversed");
  const auto utf16 = text | alef::as_utf16 | std::ranges::to<std::u16string>();
  const auto utf32 = text | alef::as_utf32 | std::ranges::to<std::u32string>();
  if (sizes(utf16) != sizes(text) || sizes(utf32) != sizes(text))
    wrong("in UTF-16 or UTF-32");
  return ok;
}

}  // namespace

// GraphemeBreakTest.txt, line by line: the code points of a line, with
// U+00F7 where there is a boundary and U+00D7 where there is none. Then text
// made of the code points the rules are about, to see the ways of reading it
// agree where the file has nothing to say.
int main(int argc, char** argv) {
  if (argc != 2) {
    std::println(std::cerr, "usage: alef-grapheme-test GraphemeBreakTest.txt");
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::println(std::cerr, "cannot open {}", argv[1]);
    return 2;
  }
  int cases = 0;
  std::string row;
  while (std::getline(in, row)) {
    if (const std::size_t hash = row.find('#'); hash != std::string::npos)
      row.resize(hash);
    std::istringstream tokens(row);
    std::u32string code_points;
    std::vector<std::size_t> marks;
    std::string token;
    while (tokens >> token) {
      if (token == "÷")
        marks.push_back(code_points.size());
      else if (token != "×")
        code_points.push_back(
            static_cast<char32_t>(std::stoul(token, nullptr, 16)));
    }
    if (marks.empty())
      continue;
    ++cases;
    // The marks count code points; the boundaries are bytes.
    std::string text;
    std::vector<std::size_t> expected;
    for (std::size_t at = 0; at <= code_points.size(); ++at) {
      if (std::ranges::contains(marks, at))
        expected.push_back(text.size());
      if (at < code_points.size())
        text += std::u32string_view(&code_points[at], 1) | alef::as_utf8 |
                std::views::transform([](char8_t unit) { return char(unit); }) |
                std::ranges::to<std::string>();
    }
    if (forwards(text) != expected)
      fail(row);
    else
      agree(text, row);
  }

  constexpr char32_t interesting[] = {
      U'a',       U'\r',      U'\n',      U'\x01',    U'̀',  // Extend
      U'‍',  U'\U0001F1E6', U'\U0001F1E7',                  // ZWJ, RI
      U'\U0001F468', U'©',                                  // pictographs
      U'؀',  U'ः',                        // Prepend, SpacingMark
      U'ᄀ',  U'ᅡ',  U'ᆨ',  U'가',  U'각',  // Hangul
      U'क',  U'्',  U'ष',  U'़',  // Devanagari
      U'\U0001F3FB',                                // an emoji modifier
  };
  std::uint64_t state = 0x2545F4914F6CDD1D;
  const auto next = [&] {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::uint32_t>(state >> 33);
  };
  for (int sample = 0; sample < 100000; ++sample) {
    std::string text;
    const std::uint32_t length = 1 + next() % 12;
    for (std::uint32_t at = 0; at < length; ++at) {
      if (next() % 16 == 0) {
        text += static_cast<char>(0x80 + next() % 0x40);  // a stray byte
      } else {
        const char32_t one = interesting[next() % std::size(interesting)];
        for (const char8_t unit : std::u32string_view(&one, 1) | alef::as_utf8)
          text += static_cast<char>(unit);
      }
    }
    if (!agree(text, std::format("random text {}", sample)))
      break;
  }

  std::println("grapheme: {} lines of the test file, {} failures", cases,
               failures);
  return failures || cases == 0 ? 1 : 0;
}
