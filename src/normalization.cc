// SPDX-License-Identifier: AGPL-3.0-only
// Normalization: the four forms of UAX #15, Unicode Normalization Forms.
//
// Text in any UTF is read as code points and given out as code points in the
// form asked for -- NFD and NFKD decomposed, canonically or by compatibility;
// NFC and NFKC decomposed and composed again -- and lazily: a piece at a
// time, each from a code point that nothing after it is reordered past or
// composed with anything before, to the next such. A piece is decomposed,
// put in canonical order, and composed again where the form is composed.
export module alef.normalization;

import std;
import alef.utf;
import alef.tables.normalization;

export namespace alef {

enum class normalization_form : std::uint8_t { nfc, nfd, nfkc, nfkd };

// The Canonical_Combining_Class property: 0 for a starter.
constexpr std::uint8_t canonical_combining_class(char32_t code_point) noexcept {
  return tables::combining_class_of(code_point);
}

}  // namespace alef

namespace alef::detail {

constexpr bool by_compatibility(normalization_form form) noexcept {
  return form == normalization_form::nfkc || form == normalization_form::nfkd;
}
constexpr bool composed(normalization_form form) noexcept {
  return form == normalization_form::nfc || form == normalization_form::nfkc;
}

// Hangul syllables are decomposed and composed by arithmetic, not from the
// tables (the Unicode Standard, section 3.12).
inline constexpr char32_t hangul_s = 0xAC00;
inline constexpr char32_t hangul_l = 0x1100;
inline constexpr char32_t hangul_v = 0x1161;
inline constexpr char32_t hangul_t = 0x11A7;
inline constexpr char32_t hangul_l_count = 19;
inline constexpr char32_t hangul_v_count = 21;
inline constexpr char32_t hangul_t_count = 28;
inline constexpr char32_t hangul_n_count = hangul_v_count * hangul_t_count;
inline constexpr char32_t hangul_s_count = hangul_l_count * hangul_n_count;

constexpr bool is_hangul_syllable(char32_t code_point) noexcept {
  return code_point >= hangul_s && code_point < hangul_s + hangul_s_count;
}

template <normalization_form Form>
constexpr std::u32string_view decomposition_of(char32_t code_point) noexcept {
  if constexpr (by_compatibility(Form))
    return tables::compatibility_decomposition(code_point);
  else
    return tables::canonical_decomposition(code_point);
}

// `code_point` decomposed the form's way, all the way down, onto `out`.
template <normalization_form Form>
constexpr void decompose(char32_t code_point, std::vector<char32_t>& out) {
  // Nothing before U+00A0 decomposes, either way.
  if (code_point < 0xA0) {
    out.push_back(code_point);
    return;
  }
  if (is_hangul_syllable(code_point)) {
    const char32_t index = code_point - hangul_s;
    out.push_back(hangul_l + index / hangul_n_count);
    out.push_back(hangul_v + index % hangul_n_count / hangul_t_count);
    if (index % hangul_t_count != 0)
      out.push_back(hangul_t + index % hangul_t_count);
    return;
  }
  const std::u32string_view mapping = decomposition_of<Form>(code_point);
  if (mapping.empty())
    out.push_back(code_point);
  else
    out.insert(out.end(), mapping.begin(), mapping.end());
}

// Whether a piece of text may end before `code_point` and the next begin
// with it: nothing after it is reordered past it, or composed with anything
// before it. Decomposed, that is a code point whose decomposition begins with
// a starter; composed, a starter the quick check says is in the form, which
// nothing before it composes with.
template <normalization_form Form>
constexpr bool boundary_before(char32_t code_point) noexcept {
  if (code_point < 0x80)
    return true;
  if constexpr (composed(Form)) {
    const tables::quick_checks checks = tables::quick_checks_of(code_point);
    const tables::quick_check check =
        by_compatibility(Form) ? checks.nfkc() : checks.nfc();
    return tables::combining_class_of(code_point) == 0 &&
           check == tables::quick_check::yes;
  } else {
    if (is_hangul_syllable(code_point))
      return true;
    const std::u32string_view mapping = decomposition_of<Form>(code_point);
    return tables::combining_class_of(mapping.empty() ? code_point
                                                      : mapping.front()) == 0;
  }
}

// Canonical order: each run of code points that are not starters sorted by
// combining class, stably.
constexpr void put_in_canonical_order(std::vector<char32_t>& text) {
  for (std::size_t at = 1; at < text.size(); ++at) {
    const char32_t one = text[at];
    const std::uint8_t combining = tables::combining_class_of(one);
    if (combining == 0)
      continue;
    std::size_t to = at;
    while (to > 0 && tables::combining_class_of(text[to - 1]) > combining) {
      text[to] = text[to - 1];
      --to;
    }
    text[to] = one;
  }
}

// What two code points compose to, or 0.
constexpr char32_t compose_pair(char32_t first, char32_t second) noexcept {
  if (first >= hangul_l && first < hangul_l + hangul_l_count &&
      second >= hangul_v && second < hangul_v + hangul_v_count)
    return hangul_s +
           ((first - hangul_l) * hangul_v_count + (second - hangul_v)) *
               hangul_t_count;
  if (is_hangul_syllable(first) && (first - hangul_s) % hangul_t_count == 0 &&
      second > hangul_t && second < hangul_t + hangul_t_count)
    return first + (second - hangul_t);
  return tables::primary_composite(first, second);
}

// Canonical composition (the Unicode Standard, section 3.11, D117): each code
// point composed with the last starter before it, where nothing between them
// blocks it -- nothing of combining class 0, or of one as high as its own --
// and the two are the canonical mapping of a primary composite.
constexpr void compose(std::vector<char32_t>& text) {
  const std::size_t none = text.size();
  std::size_t starter = none;
  std::uint8_t last = 0;  // the combining class of the last code point kept
  std::size_t kept = 0;
  for (std::size_t at = 0; at < text.size(); ++at) {
    const char32_t one = text[at];
    const std::uint8_t combining = tables::combining_class_of(one);
    if (starter != none && (kept == starter + 1 || (last != 0 && last < combining)))
      if (const char32_t composite = compose_pair(text[starter], one)) {
        text[starter] = composite;
        continue;
      }
    if (combining == 0)
      starter = kept;
    last = combining;
    text[kept++] = one;
  }
  text.resize(kept);
}

}  // namespace alef::detail

export namespace alef {

// Text in any UTF in the form Form, as code points: read a piece at a time,
// as it is stepped to, and given out from the piece. An input range, whatever
// the text is; text | alef::as_utf8 and the like make it code units again.
template <normalization_form Form, std::ranges::view V>
  requires utf_range<V>
class normalize_view
    : public std::ranges::view_interface<normalize_view<Form, V>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

  struct reading {
    I next;
    S last;
    // The code point read past the piece: the first of the next.
    std::optional<char32_t> ahead;
    std::vector<char32_t> piece;
    std::size_t given = 0;
    bool done = false;

    constexpr reading(I first, S end)
        : next(std::move(first)), last(std::move(end)) {}

    constexpr std::optional<char32_t> read() {
      if (next == last)
        return std::nullopt;
      bool well_formed = false;
      return detail::read<Unit>(next, last, well_formed);
    }

    // The next piece, in the form, into `piece`.
    constexpr void next_piece() {
      piece.clear();
      given = 0;
      if (!ahead) {
        done = true;
        return;
      }
      detail::decompose<Form>(*ahead, piece);
      ahead = std::nullopt;
      while (const std::optional<char32_t> code_point = read()) {
        if (detail::boundary_before<Form>(*code_point)) {
          ahead = code_point;
          break;
        }
        detail::decompose<Form>(*code_point, piece);
      }
      detail::put_in_canonical_order(piece);
      if constexpr (detail::composed(Form))
        detail::compose(piece);
    }
  };

 public:
  class iterator {
   public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr explicit iterator(reading* at) noexcept : at_(at) {}

    constexpr char32_t operator*() const noexcept {
      return at_->piece[at_->given];
    }
    constexpr iterator& operator++() {
      if (++at_->given == at_->piece.size())
        at_->next_piece();
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) noexcept {
      return one.at_->done;
    }

   private:
    reading* at_ = nullptr;
  };

  normalize_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit normalize_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr iterator begin() {
    reading& at =
        reading_.emplace(std::ranges::begin(base_), std::ranges::end(base_));
    at.ahead = at.read();
    at.next_piece();
    return iterator(&at);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
  detail::non_propagating<reading> reading_;
};

// text | as_nfc, or as_nfc(text); and as_nfd, as_nfkc, as_nfkd.
template <normalization_form Form>
struct normalize_fn : detail::adaptor_closure<normalize_fn<Form>> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    return normalize_view<Form, detail::all_of_t<Range>>(
        detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr normalize_fn<normalization_form::nfc> as_nfc{};
inline constexpr normalize_fn<normalization_form::nfd> as_nfd{};
inline constexpr normalize_fn<normalization_form::nfkc> as_nfkc{};
inline constexpr normalize_fn<normalization_form::nfkd> as_nfkd{};

// Whether text is in the form: the quick check of UAX #15, section 9, which
// says no or yes for most text at a code point each, and where it says maybe,
// the text against itself normalized. Text that is not well-formed is in no
// form. The text is read twice where it is maybe, so it is a forward range.
template <normalization_form Form>
struct is_normalized_fn {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range> && std::ranges::forward_range<Range>
  constexpr bool operator()(Range&& range) const {
    auto text = detail::all_of(std::forward<Range>(range));
    using Unit = detail::unit_of<decltype(text)>;
    bool maybe = false;
    std::uint8_t last = 0;
    auto at = std::ranges::begin(text);
    const auto end = std::ranges::end(text);
    while (at != end) {
      bool well_formed = false;
      const char32_t code_point = detail::read<Unit>(at, end, well_formed);
      if (!well_formed)
        return false;
      if (code_point < 0x80) {
        last = 0;
        continue;
      }
      const std::uint8_t combining = tables::combining_class_of(code_point);
      if (combining != 0 && last > combining)
        return false;
      const tables::quick_checks checks = tables::quick_checks_of(code_point);
      const tables::quick_check check =
          Form == normalization_form::nfc    ? checks.nfc()
          : Form == normalization_form::nfd  ? checks.nfd()
          : Form == normalization_form::nfkc ? checks.nfkc()
                                             : checks.nfkd();
      if (check == tables::quick_check::no)
        return false;
      if (check == tables::quick_check::maybe)
        maybe = true;
      last = combining;
    }
    return !maybe ||
           std::ranges::equal(text | as_utf32, text | normalize_fn<Form>{});
  }
};
inline constexpr is_normalized_fn<normalization_form::nfc> is_nfc{};
inline constexpr is_normalized_fn<normalization_form::nfd> is_nfd{};
inline constexpr is_normalized_fn<normalization_form::nfkc> is_nfkc{};
inline constexpr is_normalized_fn<normalization_form::nfkd> is_nfkd{};

}  // namespace alef
