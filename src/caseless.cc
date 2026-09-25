// Caseless matching beyond case alone: the Unicode Standard, chapter 3,
// D145 and D146 -- text compared ignoring case and canonical differences,
// or ignoring case and compatibility differences too. D144, the default
// caseless match, is equal_ignoring_case in alef.casing.
//
// All of it lazy: the keys are views, and matching compares them code point
// by code point, stopping at the first difference.
export module alef.caseless;

import std;
import alef.utf;
import alef.casing;
import alef.normalization;

export namespace alef {

// The key two texts are canonically caseless equal by (D145):
// NFD(toCasefold(NFD(X))). The first NFD is there for the few code points
// whose folding changes with a decomposition, U+0345 among them.
// With casing_language::turkic that NFD comes first as well: U+0130, dotted
// capital I, becomes I and U+0307 before folding, so it is dotless i with a
// dot above, not i -- only the default caseless match (equal_ignoring_case)
// folds U+0130 to i.
struct canonical_caseless_fn : std::ranges::range_adaptor_closure<canonical_caseless_fn> {
  casing_language language = casing_language::other;

  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& text) const {
    return std::forward<Range>(text) | as_nfd | as_folded(language) | as_nfd;
  }
  constexpr canonical_caseless_fn operator()(casing_language in) const noexcept {
    return {{}, in};
  }
};
inline constexpr canonical_caseless_fn as_canonical_caseless{};

// The key two texts are compatibility caseless equal by (D146):
// NFKD(toCasefold(NFKD(toCasefold(NFD(X))))).
struct compatibility_caseless_fn : std::ranges::range_adaptor_closure<compatibility_caseless_fn> {
  casing_language language = casing_language::other;

  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& text) const {
    return std::forward<Range>(text) | as_nfd | as_folded(language) | as_nfkd |
           as_folded(language) | as_nfkd;
  }
  constexpr compatibility_caseless_fn operator()(casing_language in) const noexcept {
    return {{}, in};
  }
};
inline constexpr compatibility_caseless_fn as_compatibility_caseless{};

// Whether two texts, in any UTFs, are canonically equivalent but for case.
template <std::ranges::viewable_range One, std::ranges::viewable_range Other>
  requires utf_range<One> && utf_range<Other>
constexpr bool equivalent_ignoring_case(One&& one, Other&& other,
                                        casing_language language = casing_language::other) {
  return std::ranges::equal(std::forward<One>(one) | as_canonical_caseless(language),
                            std::forward<Other>(other) | as_canonical_caseless(language));
}

// Whether two texts, in any UTFs, are compatibility equivalent but for case:
// a circled 1 matches 1, a full-width letter the letter.
template <std::ranges::viewable_range One, std::ranges::viewable_range Other>
  requires utf_range<One> && utf_range<Other>
constexpr bool compatible_ignoring_case(One&& one, Other&& other,
                                        casing_language language = casing_language::other) {
  return std::ranges::equal(std::forward<One>(one) | as_compatibility_caseless(language),
                            std::forward<Other>(other) | as_compatibility_caseless(language));
}

}  // namespace alef
