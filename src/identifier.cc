// SPDX-License-Identifier: AGPL-3.0-only
// Identifiers: UAX #31, Unicode Identifiers and Syntax, for Unicode 18.0.0 --
// the properties identifiers are made of, and the default identifier syntax
// of UAX31-D1, <Start> <Continue>*, with XID_Start and XID_Continue, which
// hold under NFKC.
export module alef.identifier;

import std;
import alef.utf;
import alef.tables.identifier;

export namespace alef {

// Whether a code point may begin an identifier (XID_Start).
constexpr bool is_xid_start(char32_t code_point) noexcept {
  return tables::xid_start(code_point);
}

// Whether a code point may go on an identifier (XID_Continue): the ones
// that begin one, and digits, combining marks and connectors like '_'.
constexpr bool is_xid_continue(char32_t code_point) noexcept {
  return tables::xid_continue(code_point);
}

// Whether a code point is Default_Ignorable_Code_Point: one a renderer draws
// as nothing where it cannot show it -- a soft hyphen, a zero-width joiner,
// a variation selector. Identifiers and skeletons pass over them.
constexpr bool is_default_ignorable(char32_t code_point) noexcept {
  return tables::default_ignorable(code_point);
}

// The end of the identifier that begins at `at`: past the longest run of
// XID_Start then XID_Continue, or `at` itself where none begins. Ill-formed
// code units end an identifier.
template <std::forward_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr I identifier_end(I at, S last) {
  using Unit = std::iter_value_t<I>;
  bool well_formed = false;
  if (at == last)
    return at;
  I after = at;
  if (!is_xid_start(detail::read<Unit>(after, last, well_formed)) || !well_formed)
    return at;
  for (at = after; at != last; at = after) {
    after = at;
    if (!is_xid_continue(detail::read<Unit>(after, last, well_formed)) || !well_formed)
      return at;
  }
  return at;
}

// Whether all of some text, in any UTF, is one identifier.
struct is_identifier_fn {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range> && std::ranges::forward_range<detail::all_of_t<Range>>
  constexpr bool operator()(Range&& text) const {
    auto all = detail::all_of(std::forward<Range>(text));
    const auto first = std::ranges::begin(all);
    const auto last = std::ranges::end(all);
    return first != last && identifier_end(first, last) == last;
  }
};
inline constexpr is_identifier_fn is_identifier{};

}  // namespace alef
