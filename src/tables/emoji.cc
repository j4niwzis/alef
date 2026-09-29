// SPDX-License-Identifier: AGPL-3.0-only
// Unicode's emoji as emoji-test.txt (UTS #51) lists them for keyboards and
// palettes: the fully-qualified ones, in CLDR's order, in their groups, with
// their names -- read from the file while this interface is compiled.
//
// The file keeps its groups and its names in comments ("# group: People &
// Body", "... ; fully-qualified # 😀 E1.0 grinning face"), which alef.ucd's
// reader of data lines passes over; so this module reads its lines itself.
// Each line is read once into what it is -- a group's start, an emoji, or
// neither -- and all that follows works on that.
//
// A module of its own that imports nothing of the library, so that it is
// compiled again when its file changes, and not when anything else does.
export module alef.tables.emoji;

import std;

export namespace alef::tables::emoji_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
inline constexpr char test_bytes[] = {
#embed "../../ucd/emoji/emoji-test.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
inline constexpr std::string_view text{test_bytes, sizeof test_bytes};

template <class... F> struct overloaded : F... {
  using F::operator()...;
};

constexpr std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

// What a line of the file is.
namespace line_kind {
// "# group: Smileys & Emotion": the emoji after it are that group's. The
// group "Component" holds the pieces emoji are made of -- the skin tones,
// the hair styles -- not emoji to be chosen, and is left out.
struct group {
  std::string_view name;
  bool kept = true;
};
// "1F600 ; fully-qualified # 😀 E1.0 grinning face": its code points, as
// written; whether it is the form to be used (the others lack a
// presentation selector somewhere); its name.
struct entry {
  std::string_view points;
  bool fully_qualified = false;
  std::string_view name;
};
// A comment, a subgroup, a blank line.
struct other {};
} // namespace line_kind
using line_t = std::variant<line_kind::group, line_kind::entry, line_kind::other>;

constexpr line_t line_of(std::string_view line) {
  constexpr std::string_view group_mark = "# group: ";
  if (line.starts_with(group_mark)) {
    const std::string_view name = trim(line.substr(group_mark.size()));
    return line_kind::group{name, name != "Component"};
  }
  if (line.empty() || line.front() == '#')
    return line_kind::other{};
  const std::size_t semicolon = line.find(';');
  const std::size_t hash = line.find('#');
  if (semicolon == std::string_view::npos || hash == std::string_view::npos || hash < semicolon)
    return line_kind::other{};
  // After '#': the emoji, its version ("E1.0"), then its name.
  const std::string_view comment = trim(line.substr(hash + 1));
  std::string_view name;
  if (const std::size_t version = comment.find(" E"); version != std::string_view::npos)
    if (const std::size_t space = comment.find(' ', version + 2); space != std::string_view::npos)
      name = trim(comment.substr(space + 1));
  return line_kind::entry{trim(line.substr(0, semicolon)),
                          trim(line.substr(semicolon + 1, hash - semicolon - 1)) == "fully-qualified", name};
}

// Each line of the file, read, in order.
template <class F> constexpr void each_line(F &&f) {
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == std::string_view::npos)
      end = text.size();
    f(line_of(text.substr(at, end - at)));
    at = end + 1;
  }
}

// Each code point of "1F44B 1F3FB", as a number.
template <class F> constexpr void each_point(std::string_view points, F &&f) {
  std::uint32_t value = 0;
  bool any = false;
  for (const char c : points) {
    if (c == ' ') {
      if (any)
        f(value);
      value = 0;
      any = false;
      continue;
    }
    const std::uint32_t digit = c >= '0' && c <= '9' ? static_cast<std::uint32_t>(c - '0')
                                : c >= 'A' && c <= 'F' ? static_cast<std::uint32_t>(c - 'A' + 10)
                                                       : static_cast<std::uint32_t>(c - 'a' + 10);
    value = value * 16 + digit;
    any = true;
  }
  if (any)
    f(value);
}
constexpr std::size_t utf8_length(std::uint32_t c) { return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4; }
// A skin tone's modifier: an emoji with one is a variant of another.
constexpr bool is_tone(std::uint32_t c) { return c >= 0x1F3FB && c <= 0x1F3FF; }

// How much there is: the groups kept that have emoji, the emoji, and the
// bytes of their UTF-8.
struct sizes_t {
  std::size_t groups = 0, entries = 0, bytes = 0;
};
constexpr sizes_t count() {
  sizes_t out;
  bool kept = false, counted = false;
  each_line([&](const line_t &line) {
    std::visit(overloaded{[&](const line_kind::group &one) {
                            kept = one.kept;
                            counted = false;
                          },
                          [&](const line_kind::entry &one) {
                            if (!kept || !one.fully_qualified)
                              return;
                            if (!counted) {
                              ++out.groups;
                              counted = true;
                            }
                            ++out.entries;
                            each_point(one.points, [&](std::uint32_t c) { out.bytes += utf8_length(c); });
                          },
                          [](const line_kind::other &) {}},
               line);
  });
  return out;
}
inline constexpr sizes_t sizes = count();

// Where each emoji's UTF-8 is in the bytes, and each group's emoji among them.
struct place {
  std::size_t offset = 0, length = 0;
  std::string_view name;
  bool toned = false;
};
struct group_place {
  std::string_view name;
  std::size_t first = 0, count = 0;
};
struct table_t {
  std::array<char, sizes.bytes> bytes{};
  std::array<place, sizes.entries> places{};
  std::array<group_place, sizes.groups> groups{};
};
constexpr table_t build() {
  table_t out;
  std::size_t byte = 0, entry = 0, group = 0;
  bool kept = false, counted = false;
  std::string_view group_name;
  each_line([&](const line_t &line) {
    std::visit(overloaded{[&](const line_kind::group &one) {
                            kept = one.kept;
                            counted = false;
                            group_name = one.name;
                          },
                          [&](const line_kind::entry &one) {
                            if (!kept || !one.fully_qualified)
                              return;
                            if (!counted) {
                              out.groups[group++] = {group_name, entry, 0};
                              counted = true;
                            }
                            place &made = out.places[entry++];
                            made.offset = byte;
                            made.name = one.name;
                            each_point(one.points, [&](std::uint32_t c) {
                              made.toned = made.toned || is_tone(c);
                              switch (utf8_length(c)) {
                              case 1:
                                out.bytes[byte++] = static_cast<char>(c);
                                break;
                              case 2:
                                out.bytes[byte++] = static_cast<char>(0xC0 | (c >> 6));
                                out.bytes[byte++] = static_cast<char>(0x80 | (c & 0x3F));
                                break;
                              case 3:
                                out.bytes[byte++] = static_cast<char>(0xE0 | (c >> 12));
                                out.bytes[byte++] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                                out.bytes[byte++] = static_cast<char>(0x80 | (c & 0x3F));
                                break;
                              default:
                                out.bytes[byte++] = static_cast<char>(0xF0 | (c >> 18));
                                out.bytes[byte++] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                                out.bytes[byte++] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                                out.bytes[byte++] = static_cast<char>(0x80 | (c & 0x3F));
                              }
                            });
                            made.length = byte - made.offset;
                            ++out.groups[group - 1].count;
                          },
                          [](const line_kind::other &) {}},
               line);
  });
  return out;
}
inline constexpr table_t table = build();

} // namespace alef::tables::emoji_data
