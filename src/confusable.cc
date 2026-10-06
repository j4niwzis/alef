// SPDX-License-Identifier: AGPL-3.0-only
// Confusable text: UTS #39, Unicode Security Mechanisms, for Unicode 18.0.0 --
// the skeleton of text, and whether two texts can be taken for each other:
// "paypal" and "pаypal", whose second a is Cyrillic.
export module alef.confusable;

import std;
import alef.utf;
import alef.normalization;
import alef.identifier;
import alef.tables.confusable;

export namespace alef {

// What a code point can be taken for: its prototype, or itself where it has
// none. A range of code points.
class prototype {
 public:
  constexpr explicit prototype(char32_t code_point) noexcept
      : mapped_(tables::prototype_of(code_point)), itself_(code_point) {}

  constexpr const char32_t* begin() const noexcept {
    return mapped_.empty() ? &itself_ : mapped_.data();
  }
  constexpr const char32_t* end() const noexcept {
    return mapped_.empty() ? &itself_ + 1 : mapped_.data() + mapped_.size();
  }

 private:
  std::span<const char32_t> mapped_;
  char32_t itself_;
};

// The skeleton of text in any UTF (UTS #39, section 4), lazily: NFD, less
// what is Default_Ignorable_Code_Point, each code point's prototype, NFD
// again. Two texts are confusable where their skeletons are equal.
struct skeleton_fn : detail::adaptor_closure<skeleton_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& text) const {
    return std::views::join(std::views::transform(std::views::filter(std::forward<Range>(text) | as_nfd, [](char32_t one) { return !is_default_ignorable(one); }), [](char32_t one) { return prototype(one); })) |
           as_nfd;
  }
};
inline constexpr skeleton_fn as_skeleton{};

// Whether two texts, in any UTFs, can be taken for each other: their
// skeletons are the same.
template <std::ranges::viewable_range One, std::ranges::viewable_range Other>
  requires utf_range<One> && utf_range<Other>
constexpr bool confusable(One&& one, Other&& other) {
  return std::ranges::equal(std::forward<One>(one) | as_skeleton,
                            std::forward<Other>(other) | as_skeleton);
}

}  // namespace alef
