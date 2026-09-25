// The Script and Script_Extensions properties, as tables read from the UCD
// while this interface is compiled: the scripts and their names from
// PropertyValueAliases.txt, each code point's script from Scripts.txt, and
// the scripts a code point is used with from ScriptExtensions.txt.
//
// A script is an index into the scripts the file names, not an enumeration:
// which scripts there are is data, and grows with every version of Unicode.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.script;

import std;
import alef.ucd;

namespace alef::tables::script_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char aliases_bytes[] = {
#embed "../../ucd/PropertyValueAliases.txt"
};
constexpr char scripts_bytes[] = {
#embed "../../ucd/Scripts.txt"
};
constexpr char extensions_bytes[] = {
#embed "../../ucd/ScriptExtensions.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file aliases_file{aliases_bytes, sizeof aliases_bytes};
constexpr ucd::file scripts_file{scripts_bytes, sizeof scripts_bytes};
constexpr ucd::file extensions_file{extensions_bytes, sizeof extensions_bytes};

// A script: its code, of ISO 15924, and its name.
struct script_names {
  std::string_view code;
  std::string_view name;
};

// The lines "sc ; Latn ; Latin" of PropertyValueAliases.txt: Unknown first,
// since it is what a code point Scripts.txt does not list is, and then every
// other in the order of the file.
constexpr bool names_a_script(const ucd::line& one) {
  return one.points == "sc" && one.count == 2 && one.fields[0] != "Zzzz";
}

inline constexpr std::size_t count = [] {
  std::size_t scripts = 1;
  ucd::each_line(aliases_file, [&](const ucd::line& one) {
    if (names_a_script(one))
      ++scripts;
  });
  return scripts;
}();
static_assert(count <= 256, "a script is an index of a byte");

inline constexpr std::array<script_names, count> names = [] {
  std::array<script_names, count> out{};
  out[0] = {"Zzzz", "Unknown"};
  std::size_t at = 1;
  ucd::each_line(aliases_file, [&](const ucd::line& one) {
    if (names_a_script(one))
      out[at++] = {one.fields[0], one.fields[1]};
  });
  return out;
}();

// The scripts sorted by name and by code, to be looked up by either.
template <auto Key>
inline constexpr std::array<std::uint8_t, count> sorted_by = [] {
  std::array<std::uint8_t, count> out{};
  for (std::size_t at = 0; at < count; ++at)
    out[at] = static_cast<std::uint8_t>(at);
  std::ranges::sort(out, {}, [](std::uint8_t index) { return names[index].*Key; });
  return out;
}();

template <auto Key>
constexpr std::optional<std::uint8_t> index_by(std::string_view key) noexcept {
  const auto& sorted = sorted_by<Key>;
  const auto found = std::ranges::lower_bound(
      sorted, key, {}, [](std::uint8_t index) { return names[index].*Key; });
  if (found == sorted.end() || names[*found].*Key != key)
    return std::nullopt;
  return *found;
}

constexpr std::optional<std::uint8_t> script_in(const ucd::line& one) {
  if (one.count != 1)
    return std::nullopt;
  return index_by<&script_names::name>(one.fields[0]);
}

constexpr auto scripts = ucd::property<std::uint8_t, scripts_file, &script_in>;

constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : scripts)
    if (one.value != 0) {
      if (!out.empty() && out.back().last + 1 == one.first &&
          out.back().value == one.value)
        out.back().last = one.last;
      else
        out.push_back({one.first, one.last, one.value});
    }
  return out;
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

// Script_Extensions: a set of scripts for each code point the file lists, in
// codes split by spaces. The same set is kept once, and a code point is the
// number of its set, from 1; 0 is a code point the file does not list.
struct extension_data {
  std::vector<ucd::run<std::uint16_t>> runs;
  std::vector<std::uint8_t> pool;
  std::vector<std::pair<std::uint16_t, std::uint8_t>> sets;  // offset, length
};

constexpr extension_data read_extensions() {
  extension_data out;
  ucd::each_line(extensions_file, [&](const ucd::line& one) {
    if (one.count != 1)
      return;
    std::vector<std::uint8_t> set;
    const std::string_view codes = one.fields[0];
    for (std::size_t start = codes.find_first_not_of(' ');
         start != std::string_view::npos;) {
      std::size_t stop = codes.find(' ', start);
      if (stop == std::string_view::npos)
        stop = codes.size();
      if (const auto index = index_by<&script_names::code>(codes.substr(start, stop - start)))
        set.push_back(*index);
      start = codes.find_first_not_of(' ', stop);
    }
    std::ranges::sort(set);
    std::size_t number = 0;
    for (std::size_t kept = 0; kept < out.sets.size() && number == 0; ++kept) {
      const auto [offset, length] = out.sets[kept];
      if (std::ranges::equal(set, std::span(out.pool).subspan(offset, length)))
        number = kept + 1;
    }
    if (number == 0) {
      out.sets.push_back({static_cast<std::uint16_t>(out.pool.size()),
                          static_cast<std::uint8_t>(set.size())});
      out.pool.insert(out.pool.end(), set.begin(), set.end());
      number = out.sets.size();
    }
    out.runs.push_back({one.first(), one.last(), static_cast<std::uint16_t>(number)});
  });
  std::ranges::sort(out.runs, {}, &ucd::run<std::uint16_t>::first);
  return out;
}

struct extension_sizes {
  std::size_t blocks = 0;
  std::size_t pool = 0;
  std::size_t sets = 0;
};

inline constexpr extension_sizes extension_sized = [] {
  const extension_data data = read_extensions();
  return extension_sizes{ucd::lay_out(data.runs).blocks.size() / ucd::block_size,
                         data.pool.size(), data.sets.size()};
}();

struct extension_tables {
  ucd::two_stage_table<std::uint16_t, extension_sized.blocks> table;
  std::array<std::uint8_t, extension_sized.pool> pool;
  std::array<std::pair<std::uint16_t, std::uint8_t>, extension_sized.sets> sets;
};

inline constexpr extension_tables extensions = [] {
  const extension_data data = read_extensions();
  extension_tables out{};
  out.table = ucd::two_stage<std::uint16_t, extension_sized.blocks>(
      ucd::lay_out(data.runs));
  std::ranges::copy(data.pool, out.pool.begin());
  std::ranges::copy(data.sets, out.sets.begin());
  return out;
}();

}  // namespace alef::tables::script_data

export namespace alef::tables {

constexpr std::size_t script_count() noexcept { return script_data::count; }

constexpr std::string_view script_code(std::uint8_t index) noexcept {
  return script_data::names[index].code;
}
constexpr std::string_view script_name(std::uint8_t index) noexcept {
  return script_data::names[index].name;
}

// The index of the script with this name or code; Unknown's, 0, where no
// script has it.
constexpr std::uint8_t script_index(std::string_view name_or_code) noexcept {
  if (const auto index =
          script_data::index_by<&script_data::script_names::name>(name_or_code))
    return *index;
  return script_data::index_by<&script_data::script_names::code>(name_or_code)
      .value_or(0);
}

constexpr std::uint8_t script_of(char32_t code_point) noexcept {
  return script_data::table[code_point];
}

// The scripts Script_Extensions gives `code_point`, where ScriptExtensions.txt
// lists it; nothing where it does not, and its script is all there is.
constexpr std::span<const std::uint8_t> script_extensions_of(
    char32_t code_point) noexcept {
  const std::uint16_t number = script_data::extensions.table[code_point];
  if (number == 0)
    return {};
  const auto [offset, length] = script_data::extensions.sets[number - 1];
  return {script_data::extensions.pool.data() + offset, length};
}

}  // namespace alef::tables
