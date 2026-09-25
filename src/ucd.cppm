// The Unicode Character Database, read at compile time from its own files.
//
// The files in ucd/ are the ones the Unicode Consortium publishes, as they
// are published. A property is read from them the way it is written there:
// a code point or a range of them, then the fields that say what they are.
//
//   0600..0605    ; Prepend # Cf   [6] ARABIC NUMBER SIGN..ARABIC NUMBER MARK ABOVE
//   094D          ; InCB; Linker # Mn       DEVANAGARI SIGN VIRAMA
//
// Everything after '#' is a comment, and so is a line that begins with one --
// the "# @missing" lines among them, which say what a code point not listed
// is, and which the caller of this says itself.
export module alef:ucd;

import std;

namespace alef::ucd {

// A property file, as #embed put it into the module: bytes, held as char.
//
// Not char8_t, which is what they are. #embed into a char8_t array is an
// EmbedExpr, and clang 22 writes an EmbedExpr into a compiled module
// interface in a form it cannot read back: the first constant expression in
// an importing translation unit that reaches the array crashes the compiler
// (llvm/llvm-project#195350, fixed for clang 23). An array of char whose only
// initializer is the #embed is kept as a string literal instead, and reads
// back. Every byte the parser looks at is ASCII, so nothing is lost by it.
using file = std::string_view;

// One line of data.
struct line {
  char32_t first = 0;
  char32_t last = 0;
  // The fields after the code points, trimmed: a value, or the name of a
  // property and then its value.
  std::array<std::string_view, 2> fields{};
  std::size_t count = 0;
};

constexpr std::string_view trim(std::string_view text) noexcept {
  const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
  while (!text.empty() && blank(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && blank(text.back()))
    text.remove_suffix(1);
  return text;
}

constexpr char32_t hex(std::string_view digits) noexcept {
  char32_t value = 0;
  for (const char digit : digits) {
    value <<= 4;
    if (digit >= '0' && digit <= '9')
      value |= digit - '0';
    else if (digit >= 'A' && digit <= 'F')
      value |= digit - 'A' + 10;
    else if (digit >= 'a' && digit <= 'f')
      value |= digit - 'a' + 10;
  }
  return value;
}

// Calls `visit` with each line of data in `text`.
template <class Visit>
constexpr void each_line(file text, Visit&& visit) {
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == file::npos)
      end = text.size();
    std::string_view row = text.substr(at, end - at);
    at = end + 1;
    if (const std::size_t hash = row.find('#'); hash != file::npos)
      row = row.substr(0, hash);
    row = trim(row);
    if (row.empty())
      continue;
    line one;
    std::size_t semicolon = row.find(';');
    const std::string_view points = trim(row.substr(0, semicolon));
    if (const std::size_t dots = points.find(".."); dots != file::npos) {
      one.first = hex(points.substr(0, dots));
      one.last = hex(points.substr(dots + 2));
    } else {
      one.first = one.last = hex(points);
    }
    while (semicolon != file::npos && one.count < one.fields.size()) {
      const std::size_t next = row.find(';', semicolon + 1);
      const std::size_t length =
          next == file::npos ? file::npos : next - semicolon - 1;
      one.fields[one.count++] = trim(row.substr(semicolon + 1, length));
      semicolon = next;
    }
    visit(one);
  }
}

// Code points from `first` to `last` that have `value`.
template <class Value>
struct range {
  char32_t first = 0;
  char32_t last = 0;
  Value value{};
};

// How many lines of `text` Select keeps: it answers std::optional<Value>.
template <class Value, auto Select>
constexpr std::size_t count(file text) {
  std::size_t kept = 0;
  each_line(text, [&](const line& one) {
    if (Select(one))
      ++kept;
  });
  return kept;
}

// The lines of `text` Select keeps, as ranges. N is what count() said.
template <class Value, std::size_t N, auto Select>
constexpr std::array<range<Value>, N> collect(file text) {
  std::array<range<Value>, N> out{};
  std::size_t kept = 0;
  each_line(text, [&](const line& one) {
    if (const std::optional<Value> value = Select(one))
      out[kept++] = {one.first, one.last, *value};
  });
  return out;
}

// A property: the lines of Text that Select keeps, as ranges sorted by where
// they begin. Counted, then collected: two constant expressions, each reading
// the file once, and each within the bound CMakeLists.txt raises.
template <class Value, const file& Text, auto Select>
inline constexpr std::size_t property_count = count<Value, Select>(Text);

template <class Value, const file& Text, auto Select>
inline constexpr auto property = [] {
  auto out = collect<Value, property_count<Value, Text, Select>, Select>(Text);
  std::ranges::sort(out, {}, &range<Value>::first);
  return out;
}();

// The value `code_point` has in sorted `ranges`, or `otherwise`.
template <class Value, std::size_t N>
constexpr Value lookup(const std::array<range<Value>, N>& ranges,
                       char32_t code_point, Value otherwise) noexcept {
  const auto after = std::ranges::upper_bound(ranges, code_point, {},
                                              &range<Value>::first);
  if (after == ranges.begin())
    return otherwise;
  const range<Value>& one = *std::ranges::prev(after);
  return code_point <= one.last ? one.value : otherwise;
}

}  // namespace alef::ucd
