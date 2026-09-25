// The confusable mappings of UTS #39, Unicode Security Mechanisms, for
// Unicode 18.0.0: each code point's prototype -- the string it can be taken
// for -- read from confusables.txt while this interface is compiled.
//
// A module of its own that imports nothing of the library, so that it is
// compiled again when its file changes, and not when anything else does.
export module alef.tables.confusable;

import std;

namespace alef::tables::confusable_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char confusables_bytes[] = {
#embed "../../ucd/security/confusables.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

struct entry {
  char32_t source = 0;
  std::uint32_t first = 0;
  std::uint8_t length = 0;
};

struct parsed {
  std::vector<entry> entries;
  std::vector<char32_t> pool;
};

constexpr bool blank(char one) { return one == ' ' || one == '\t' || one == '\r'; }

constexpr std::string_view trimmed(std::string_view text) {
  while (!text.empty() && blank(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && blank(text.back()))
    text.remove_suffix(1);
  return text;
}

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// A line is: source ; prototype, code points in hex ; type # comment.
constexpr parsed parse() {
  std::string_view file(confusables_bytes, sizeof confusables_bytes);
  if (file.starts_with("\xEF\xBB\xBF"))
    file.remove_prefix(3);
  parsed out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view data = file.substr(start, end - start).substr(0, file.substr(start, end - start).find('#'));
    start = end + 1;
    const std::size_t one = data.find(';');
    if (one == std::string_view::npos)
      continue;
    const std::size_t two = data.find(';', one + 1);
    if (two == std::string_view::npos)
      continue;
    entry row{hex(trimmed(data.substr(0, one))), static_cast<std::uint32_t>(out.pool.size()), 0};
    std::string_view target = trimmed(data.substr(one + 1, two - one - 1));
    while (!target.empty()) {
      std::size_t stop = 0;
      while (stop < target.size() && !blank(target[stop]))
        ++stop;
      out.pool.push_back(hex(target.substr(0, stop)));
      ++row.length;
      target = trimmed(target.substr(stop));
    }
    out.entries.push_back(row);
  }
  std::ranges::sort(out.entries, {}, &entry::source);
  return out;
}

inline constexpr std::size_t entry_count = parse().entries.size();
inline constexpr std::size_t pool_size = parse().pool.size();

struct laid_out {
  std::array<entry, entry_count> entries{};
  std::array<char32_t, pool_size> pool{};
};

inline constexpr laid_out table = [] {
  laid_out out;
  const parsed all = parse();
  std::ranges::copy(all.entries, out.entries.begin());
  std::ranges::copy(all.pool, out.pool.begin());
  return out;
}();

}  // namespace alef::tables::confusable_data

export namespace alef::tables {

// A code point's prototype, or nothing where it has none.
constexpr std::span<const char32_t> prototype_of(char32_t code_point) noexcept {
  const auto& all = confusable_data::table.entries;
  const auto found = std::ranges::lower_bound(all, code_point, {}, &confusable_data::entry::source);
  if (found == all.end() || found->source != code_point)
    return {};
  return {confusable_data::table.pool.data() + found->first, found->length};
}

}  // namespace alef::tables
