// General properties of code points -- General_Category, Script,
// Script_Extensions and East_Asian_Width -- and how many columns text takes
// where it is set in columns, a terminal say.
export module alef.properties;

import std;
import alef.utf;
import alef.grapheme;
import alef.tables.general_category;
import alef.tables.east_asian_width;
import alef.tables.script;
import alef.tables.width;

export namespace alef {

using alef::general_category;
using alef::east_asian_width;

constexpr general_category general_category_of(char32_t code_point) noexcept {
  return tables::general_category_of(code_point);
}

constexpr east_asian_width east_asian_width_of(char32_t code_point) noexcept {
  return tables::east_asian_width_of(code_point);
}

// A script, as the Script property has them: one of the scripts
// PropertyValueAliases.txt names, found by its name or by its code of
// ISO 15924 -- script("Latin") and script("Latn") are the same. What no
// script is named is Unknown, and so is a script made of nothing.
class script {
 public:
  constexpr script() noexcept = default;
  constexpr explicit script(std::string_view name_or_code) noexcept
      : index_(tables::script_index(name_or_code)) {}

  constexpr std::string_view name() const noexcept {
    return tables::script_name(index_);
  }
  constexpr std::string_view code() const noexcept {
    return tables::script_code(index_);
  }

  // Every script there is, Unknown first.
  static constexpr std::size_t count() noexcept { return tables::script_count(); }
  static constexpr script at(std::size_t index) noexcept {
    script one;
    one.index_ = static_cast<std::uint8_t>(index);
    return one;
  }

  friend constexpr bool operator==(script, script) noexcept = default;

 private:
  std::uint8_t index_ = 0;
};

constexpr script script_of(char32_t code_point) noexcept {
  return script::at(tables::script_of(code_point));
}

// The scripts a code point is used with, as Script_Extensions has them: the
// ones ScriptExtensions.txt lists for it, or its script where it lists none.
class script_set {
 public:
  class iterator {
   public:
    using value_type = script;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr iterator(const script_set* set, std::size_t at) noexcept
        : set_(set), at_(at) {}

    constexpr script operator*() const noexcept { return (*set_)[at_]; }
    constexpr iterator& operator++() noexcept {
      ++at_;
      return *this;
    }
    constexpr iterator operator++(int) noexcept {
      iterator was = *this;
      ++at_;
      return was;
    }
    friend constexpr bool operator==(const iterator&, const iterator&) = default;

   private:
    const script_set* set_ = nullptr;
    std::size_t at_ = 0;
  };

  constexpr script_set(std::span<const std::uint8_t> listed,
                       std::uint8_t own) noexcept
      : listed_(listed), own_(own) {}

  constexpr std::size_t size() const noexcept {
    return listed_.empty() ? 1 : listed_.size();
  }
  constexpr script operator[](std::size_t at) const noexcept {
    return script::at(listed_.empty() ? own_ : listed_[at]);
  }
  constexpr bool contains(script one) const noexcept {
    for (std::size_t at = 0; at < size(); ++at)
      if ((*this)[at] == one)
        return true;
    return false;
  }
  constexpr iterator begin() const noexcept { return {this, 0}; }
  constexpr iterator end() const noexcept { return {this, size()}; }

 private:
  std::span<const std::uint8_t> listed_;
  std::uint8_t own_ = 0;
};

constexpr script_set script_extensions_of(char32_t code_point) noexcept {
  return {tables::script_extensions_of(code_point), tables::script_of(code_point)};
}

// How wide what is ambiguous is (UAX #11): one column, as outside East Asian
// text, or two, as in it.
enum class ambiguous_width : std::uint8_t { narrow, wide };

// The columns a code point takes: 0, 1 or 2.
constexpr int width_of(char32_t code_point,
                       ambiguous_width ambiguous = ambiguous_width::narrow) noexcept {
  const int columns = tables::columns_of(code_point);
  if (columns >= 0)
    return columns;
  return ambiguous == ambiguous_width::wide ? 2 : 1;
}

// The columns a grapheme cluster takes: its first code point's -- but two
// where U+FE0F asks for it to be presented as emoji, and one where U+FE0E
// asks for a pictograph to be presented as text.
template <std::ranges::input_range Cluster>
  requires utf_range<Cluster>
constexpr int grapheme_width(Cluster&& cluster,
                             ambiguous_width ambiguous = ambiguous_width::narrow) {
  int columns = -1;
  bool pictograph = false;
  for (const char32_t code_point : std::forward<Cluster>(cluster) | as_utf32) {
    if (columns < 0) {
      columns = width_of(code_point, ambiguous);
      pictograph = is_extended_pictographic(code_point);
    } else if (code_point == 0xFE0F && columns == 1) {
      columns = 2;
    } else if (code_point == 0xFE0E && columns == 2 && pictograph) {
      columns = 1;
    }
  }
  return columns < 0 ? 0 : columns;
}

// The columns text in any UTF takes: its grapheme clusters', added up.
struct width_fn {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr std::size_t operator()(
      Range&& text, ambiguous_width ambiguous = ambiguous_width::narrow) const {
    std::size_t total = 0;
    for (auto&& cluster : std::forward<Range>(text) | graphemes)
      total += static_cast<std::size_t>(grapheme_width(cluster, ambiguous));
    return total;
  }
};
inline constexpr width_fn width{};

}  // namespace alef
