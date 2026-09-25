import std;
import alef;

// A few at compile time.
static_assert(std::ranges::distance(alef::graphemes(std::u8string_view(u8"é"))) == 1);
static_assert(std::ranges::distance(alef::graphemes(std::u8string_view(u8"\r\n"))) == 1);
// Regional indicators go in pairs: four of them are two clusters.
static_assert(std::ranges::distance(alef::graphemes(std::u8string_view(u8"\U0001F1E6\U0001F1E7\U0001F1E6\U0001F1E7"))) == 2);
// A family: pictographs joined by ZWJ.
static_assert(std::ranges::distance(alef::graphemes(std::u8string_view(u8"\U0001F468‍\U0001F469‍\U0001F467"))) == 1);
// ksha: consonant, virama, consonant (GB9c).
static_assert(std::ranges::distance(alef::graphemes(std::u8string_view(u8"क्ष"))) == 1);
// The example in the README.
static_assert(std::ranges::distance(alef::graphemes(u8"e\u0301\U0001F9D1\u200D\U0001F4BB\u0915\u094D\u0937")) == 3);

// GraphemeBreakTest.txt, line by line: the code points of a line, with
// U+00F7 where there is a boundary and U+00D7 where there is none.
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
  int failures = 0;
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
      else if (token != "×") {
        const alef::encoded bytes =
            alef::encode(static_cast<char32_t>(std::stoul(token, nullptr, 16)));
        text.append(reinterpret_cast<const char*>(bytes.bytes.data()),
                    bytes.length);
      }
    }
    if (expected.empty())
      continue;
    ++cases;
    std::vector<std::size_t> found{0};
    for (const std::string_view cluster : alef::graphemes(text))
      found.push_back(found.back() + cluster.size());
    if (found != expected) {
      ++failures;
      if (failures <= 20)
        std::println(std::cerr, "FAIL: {}", row);
    }
  }
  std::println("grapheme: {} of {} lines of the test file", cases - failures,
               cases);
  return failures || cases == 0 ? 1 : 0;
}
