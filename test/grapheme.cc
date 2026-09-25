import std;
import alef;

// A few at compile time.
static_assert(std::ranges::distance(alef::graphemes(u8"é")) == 1);
static_assert(std::ranges::distance(alef::graphemes("\r\n")) == 1);
// Regional indicators go in pairs: four of them are two clusters.
static_assert(std::ranges::distance(alef::graphemes(u8"\U0001F1E6\U0001F1E7\U0001F1E6\U0001F1E7")) == 2);
// A family: pictographs joined by ZWJ.
static_assert(std::ranges::distance(alef::graphemes(u8"\U0001F468‍\U0001F469‍\U0001F467")) == 1);
// ksha: consonant, virama, consonant (GB9c).
static_assert(std::ranges::distance(alef::graphemes(u8"क्ष")) == 1);
// The example in the README.
static_assert(std::ranges::distance(u8"é\U0001F9D1\u200D\U0001F4BBक्ष" | alef::graphemes) == 3);
static_assert(alef::prev_grapheme_boundary(u8"aé", 4) == 1);
static_assert(alef::is_grapheme_boundary(u8"aé", 1));
static_assert(!alef::is_grapheme_boundary(u8"aé", 2));
static_assert(!alef::is_grapheme_boundary(u8"aé", 3));  // inside U+0301
static_assert(std::ranges::distance(std::views::reverse(alef::graphemes(u8"aé"))) == 2);
static_assert(alef::grapheme_cluster_break_of(U'́') == alef::grapheme_cluster_break::extend);
static_assert(alef::indic_conjunct_break_of(U'्') == alef::indic_conjunct_break::linker);
static_assert(alef::is_extended_pictographic(U'\U0001F389'));
// A view of a temporary string would outlive its bytes, so there is none.
static_assert(std::invocable<decltype(alef::graphemes), std::string&>);
static_assert(!std::invocable<decltype(alef::graphemes), std::string>);
static_assert(std::invocable<decltype(alef::graphemes), std::string_view>);
static_assert(!std::invocable<decltype(alef::code_points), std::u8string>);

namespace {

int failures = 0;

void fail(std::string_view what) {
  if (++failures <= 20)
    std::println(std::cerr, "FAIL: {}", what);
}

std::vector<std::size_t> forwards(std::string_view text) {
  std::vector<std::size_t> found{0};
  for (const std::string_view cluster : alef::graphemes(text))
    found.push_back(found.back() + cluster.size());
  return found;
}

std::vector<std::size_t> backwards(std::string_view text) {
  std::vector<std::size_t> found{text.size()};
  while (found.back() > 0)
    found.push_back(alef::prev_grapheme_boundary(text, found.back()));
  std::ranges::reverse(found);
  if (text.empty())
    found = {0};
  return found;
}

std::vector<std::size_t> everywhere(std::string_view text) {
  std::vector<std::size_t> found;
  for (std::size_t at = 0; at <= text.size(); ++at)
    if (alef::is_grapheme_boundary(text, at))
      found.push_back(at);
  return found;
}

std::vector<std::string_view> reversed(std::string_view text) {
  std::vector<std::string_view> out;
  for (const std::string_view cluster :
       std::views::reverse(alef::graphemes(text)))
    out.push_back(cluster);
  std::ranges::reverse(out);
  return out;
}

void append(std::string& text, char32_t code_point) {
  const alef::encoded bytes = alef::encode(code_point);
  text.append(reinterpret_cast<const char*>(bytes.bytes.data()), bytes.length);
}

// Every way of finding the boundaries of `text` finds the same ones.
bool agree(std::string_view text, std::string_view what) {
  const std::vector<std::size_t> expected = forwards(text);
  bool ok = true;
  if (backwards(text) != expected) {
    fail(std::format("{}: backwards", what));
    ok = false;
  }
  if (everywhere(text) != expected) {
    fail(std::format("{}: is_grapheme_boundary", what));
    ok = false;
  }
  std::vector<std::string_view> clusters;
  for (const std::string_view cluster : alef::graphemes(text))
    clusters.push_back(cluster);
  if (reversed(text) != clusters) {
    fail(std::format("{}: reversed", what));
    ok = false;
  }
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
    std::string text;
    std::vector<std::size_t> expected;
    std::string token;
    while (tokens >> token) {
      if (token == "÷")
        expected.push_back(text.size());
      else if (token != "×")
        append(text, static_cast<char32_t>(std::stoul(token, nullptr, 16)));
    }
    if (expected.empty())
      continue;
    ++cases;
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
      if (next() % 16 == 0)
        text += static_cast<char>(0x80 + next() % 0x40);  // a stray byte
      else
        append(text, interesting[next() % std::size(interesting)]);
    }
    if (!agree(text, std::format("random text {}", sample)))
      break;
  }

  std::println("grapheme: {} lines of the test file, {} failures", cases,
               failures);
  return failures || cases == 0 ? 1 : 0;
}
