// SPDX-License-Identifier: AGPL-3.0-only
// Unicode's emoji, to be chosen from (UTS #51's emoji-test.txt): each in its
// UTF-8, with its name and whether it is a skin tone's variant of another,
// in CLDR's order and its groups -- as a keyboard or a palette lists them.
// All of it made while this interface is compiled, from the file as Unicode
// publishes it.
export module alef.emoji;

import std;
import alef.tables.emoji;

export namespace alef {

// One emoji: its text, its name ("grinning face"), and whether it is a
// variant of another for a skin tone -- which a palette shows through that
// other, not beside it.
struct emoji {
  std::string_view text;
  std::string_view name;
  bool toned = false;
};
// A group of them, as the file names it: "Smileys & Emotion", "Flags".
struct emoji_group {
  std::string_view name;
  std::span<const emoji> all;
};

} // namespace alef

namespace alef::emoji_detail {
namespace data = tables::emoji_data;
inline constexpr std::array<emoji, data::sizes.entries> all = [] {
  std::array<emoji, data::sizes.entries> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    const data::place &one = data::table_now().places[i];
    out[i] = {std::string_view(data::table_now().bytes.data() + one.offset, one.length), one.name, one.toned};
  }
  return out;
}();
inline constexpr std::array<emoji_group, data::sizes.groups> groups = [] {
  std::array<emoji_group, data::sizes.groups> out{};
  for (std::size_t g = 0; g < out.size(); ++g) {
    const data::group_place &one = data::table_now().groups[g];
    out[g] = {one.name, std::span<const emoji>(all.data() + one.first, one.count)};
  }
  return out;
}();
// The lists as read at run time: from this module's own object, where they
// were made once. A module file keeps a constant's value only where it is a
// number (clang's ASTRecordWriter::AddVarDeclInit), so a unit that read them
// made them again from the file.
std::span<const emoji> all_at_run_time() noexcept { return all; }
std::span<const emoji_group> groups_at_run_time() noexcept { return groups; }
} // namespace alef::emoji_detail

export namespace alef {

// Every emoji, in order; and the same, in their groups. In a constant
// evaluation, the lists themselves.
constexpr std::span<const emoji> emoji_all() noexcept {
  if consteval {
    return emoji_detail::all;
  } else {
    return emoji_detail::all_at_run_time();
  }
}
constexpr std::span<const emoji_group> emoji_groups() noexcept {
  if consteval {
    return emoji_detail::groups;
  } else {
    return emoji_detail::groups_at_run_time();
  }
}

} // namespace alef
