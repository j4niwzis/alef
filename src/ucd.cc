// SPDX-License-Identifier: AGPL-3.0-only
// The Unicode Character Database, read at compile time from its own files,
// and laid out as the tables the library looks code points up in.
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
//
// This module reads and lays out, and holds no data. Each table is a module
// of its own, which imports this one and embeds the files it is read from,
// and nothing else: a table is compiled again when its files or this module
// change, and not when anything else in the library does.
export module alef.ucd;

import std;

export namespace alef::ucd {

// A file of the UCD, as #embed put it into a module: bytes, held as char.
//
// Not char8_t, which is what they are. #embed into a char8_t array is an
// EmbedExpr, and clang 22 writes an EmbedExpr into a compiled module
// interface in a form it cannot read back: the first constant expression in
// an importing translation unit that reaches the array crashes the compiler
// (llvm/llvm-project#195350, fixed for clang 23). An array of char whose only
// initializer is the #embed is kept as a string literal instead, and reads
// back. Every byte the parser looks at is ASCII, so nothing is lost by it.
using file = std::string_view;

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

// One line of data: its code points as written, and the fields after them,
// trimmed -- a value, or the name of a property and then its value. The code
// points are read only when asked for: most lines of most files are passed
// over for what their fields say, which is cheaper to read than where they
// are.
struct line {
  std::string_view points;
  std::array<std::string_view, 2> fields{};
  std::size_t count = 0;

  constexpr char32_t first() const noexcept {
    return hex(points.substr(0, points.find("..")));
  }
  constexpr char32_t last() const noexcept {
    const std::size_t dots = points.find("..");
    return hex(dots == std::string_view::npos ? points : points.substr(dots + 2));
  }
};

// Calls `visit` with each line of data in `text`. Lines are found with
// find(), which a compiler evaluates as memchr rather than a character at a
// time, and a comment is passed over at its first character.
template <class Visit>
constexpr void each_line(file text, Visit&& visit) {
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == file::npos)
      end = text.size();
    std::string_view row = text.substr(at, end - at);
    at = end + 1;
    if (row.empty() || row.front() == '#')
      continue;
    if (const std::size_t hash = row.find('#'); hash != file::npos)
      row = row.substr(0, hash);
    std::size_t semicolon = row.find(';');
    if (semicolon == file::npos)
      continue;
    line one;
    one.points = trim(row.substr(0, semicolon));
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
      out[kept++] = {one.first(), one.last(), *value};
  });
  return out;
}

// A property: the lines of Text that Select keeps, as ranges sorted by where
// they begin. Counted, then collected: two constant expressions, each reading
// the file once.
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

// Code points from `first` to `last` with one value: what a table is laid
// out from, in order and apart.
template <class Value>
struct run {
  char32_t first = 0;
  char32_t last = 0;
  Value value{};
};

// What several properties say together: runs of each, in order, merged into
// runs of the values or-ed, where they are not Value{}. Walked side by side,
// once, from one place where any of them changes to the next.
template <class Value>
constexpr std::vector<run<Value>> merged(
    const std::vector<std::vector<run<Value>>>& lists) {
  std::vector<run<Value>> out;
  std::vector<std::size_t> in(lists.size(), 0);
  for (char32_t code_point = 0; code_point <= 0x10FFFF;) {
    char32_t next = 0x110000;
    Value value{};
    for (std::size_t list = 0; list < lists.size(); ++list) {
      const std::vector<run<Value>>& runs = lists[list];
      std::size_t& index = in[list];
      while (index < runs.size() && runs[index].last < code_point)
        ++index;
      if (index == runs.size())
        continue;
      if (runs[index].first > code_point) {
        next = std::min(next, runs[index].first);
        continue;
      }
      next = std::min(next, static_cast<char32_t>(runs[index].last + 1));
      value = static_cast<Value>(value | runs[index].value);
    }
    const auto last = static_cast<char32_t>(next - 1);
    if (value != Value{}) {
      if (!out.empty() && out.back().last + 1 == code_point &&
          out.back().value == value)
        out.back().last = last;
      else
        out.push_back({code_point, last, value});
    }
    code_point = next;
  }
  return out;
}

// A table of two stages, which makes what a code point is two reads and no
// search: Unicode in blocks of 256 code points, each block the index of the
// block of values it is the same as, and those blocks, each kept once.
inline constexpr std::size_t block_size = 256;
inline constexpr std::size_t block_count = 0x110000 / block_size;

template <class Value>
struct layout {
  std::vector<std::size_t> index;
  std::vector<Value> blocks;
};

// Laid out from runs in order and apart; a code point in none is Value{}.
template <class Value>
constexpr layout<Value> lay_out(const std::vector<run<Value>>& runs) {
  layout<Value> out;
  out.index.reserve(block_count);
  // A sum of each block kept, so that few are compared in full; and the
  // block that is one value throughout, for each value one is.
  std::vector<std::uint64_t> sums;
  std::vector<std::pair<Value, std::size_t>> throughout;
  std::array<Value, block_size> values{};
  const auto keep = [&] {
    std::uint64_t sum = 0;
    for (const Value value : values)
      sum = sum * 31 + static_cast<std::uint64_t>(value);
    for (std::size_t kept = 0; kept < sums.size(); ++kept)
      if (sums[kept] == sum &&
          std::ranges::equal(values, std::span(out.blocks)
                                         .subspan(kept * block_size, block_size)))
        return kept;
    sums.push_back(sum);
    out.blocks.insert(out.blocks.end(), values.begin(), values.end());
    return sums.size() - 1;
  };
  std::size_t at = 0;  // the first run that does not end before the block
  for (std::size_t block = 0; block < block_count; ++block) {
    const auto start = static_cast<char32_t>(block * block_size);
    const auto end = static_cast<char32_t>(start + block_size - 1);
    while (at < runs.size() && runs[at].last < start)
      ++at;
    // One value throughout: no run in it, or one run over all of it --
    // which is most of Unicode, and is not written out to be found so.
    std::optional<Value> same;
    if (at == runs.size() || runs[at].first > end)
      same = Value{};
    else if (runs[at].first <= start && runs[at].last >= end)
      same = runs[at].value;
    if (same) {
      auto found = std::ranges::find(throughout, *same,
                                     &std::pair<Value, std::size_t>::first);
      if (found == throughout.end()) {
        values.fill(*same);
        throughout.push_back({*same, keep()});
        found = throughout.end() - 1;
      }
      out.index.push_back(found->second);
      continue;
    }
    values.fill(Value{});
    for (std::size_t one = at; one < runs.size() && runs[one].first <= end; ++one)
      for (char32_t code_point = std::max(runs[one].first, start);
           code_point <= std::min(runs[one].last, end); ++code_point)
        values[code_point - start] = runs[one].value;
    out.index.push_back(keep());
  }
  return out;
}

template <class Value, std::size_t Blocks>
struct two_stage_table {
  static_assert(Blocks <= 65536);
  using index_type =
      std::conditional_t<(Blocks <= 256), std::uint8_t, std::uint16_t>;

  std::array<index_type, block_count> index{};
  std::array<Value, Blocks * block_size> values{};

  constexpr Value operator[](char32_t code_point) const noexcept {
    if (code_point > 0x10FFFF)
      return Value{};
    return values[std::size_t{index[code_point / block_size]} * block_size +
                  code_point % block_size];
  }
};

// Blocks is how many distinct blocks `laid` has: laid.blocks.size() /
// block_size, a constant expression of its own.
template <class Value, std::size_t Blocks>
constexpr two_stage_table<Value, Blocks> two_stage(const layout<Value>& laid) {
  using table = two_stage_table<Value, Blocks>;
  table out{};
  for (std::size_t block = 0; block < block_count; ++block)
    out.index[block] = static_cast<typename table::index_type>(laid.index[block]);
  std::ranges::copy(laid.blocks, out.values.begin());
  return out;
}

}  // namespace alef::ucd
