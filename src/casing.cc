// Case: text in lowercase, uppercase or titlecase, and folded for comparing
// without case -- by the full mappings of the Unicode Standard, section 3.13,
// with the conditions of SpecialCasing.txt: Final_Sigma always, and those of
// Lithuanian, Turkish and Azeri where the text is asked to be in one of them.
export module alef.casing;

import std;
import alef.utf;
import alef.word;
import alef.tables.casing;
import alef.tables.normalization;

export namespace alef {

// One code point to one: the simple mappings.
constexpr char32_t to_lower(char32_t code_point) noexcept {
  return tables::simple(tables::casing::lower, code_point);
}
constexpr char32_t to_upper(char32_t code_point) noexcept {
  return tables::simple(tables::casing::upper, code_point);
}
constexpr char32_t to_title(char32_t code_point) noexcept {
  return tables::simple(tables::casing::title, code_point);
}
constexpr char32_t fold(char32_t code_point) noexcept {
  return tables::simple(tables::casing::fold, code_point);
}

constexpr bool is_cased(char32_t code_point) noexcept {
  return tables::is_cased(code_point);
}
constexpr bool is_case_ignorable(char32_t code_point) noexcept {
  return tables::is_case_ignorable(code_point);
}

// The language, where case differs by it: SpecialCasing.txt has Lithuanian,
// and Turkish and Azeri, which are the same.
enum class casing_language : std::uint8_t { other, lithuanian, turkic };

}  // namespace alef

namespace alef::detail::casing_rules {

using tables::casing;

constexpr std::uint8_t combining(char32_t code_point) noexcept {
  return tables::combining_class_of(code_point);
}

// What the contexts of SpecialCasing.txt (the Unicode Standard, table 3-17)
// ask of the text before a code point, carried forward.
struct before {
  // Final_Sigma: a cased letter, then any number of case-ignorables.
  bool cased = false;
  // After_Soft_Dotted: a Soft_Dotted, then nothing of class 0 or 230.
  bool soft_dotted = false;
  // After_I: an I, then nothing of class 0 or 230.
  bool capital_i = false;

  constexpr void take(char32_t code_point) noexcept {
    if (tables::is_cased(code_point))
      cased = true;
    else if (!tables::is_case_ignorable(code_point))
      cased = false;
    const std::uint8_t cls = combining(code_point);
    const bool passes = cls != 0 && cls != 230;
    soft_dotted = tables::is_soft_dotted(code_point) || (soft_dotted && passes);
    capital_i = code_point == U'I' || (capital_i && passes);
  }
};

// `code_point` mapped the way `which` says, with what is known of the text
// before it, onto `out`. `ahead(n)` is the n-th code point after it, or
// nothing past the end, read only where a condition asks.
template <class Ahead>
constexpr void map(casing which, char32_t code_point, alef::casing_language language,
                   const before& context, Ahead&& ahead, std::vector<char32_t>& out) {
  using alef::casing_language;
  const auto put = [&](std::initializer_list<char32_t> units) {
    out.insert(out.end(), units.begin(), units.end());
  };
  // Final_Sigma: not followed by case-ignorables and then a cased letter.
  const auto followed_by_cased = [&] {
    for (std::size_t n = 0;; ++n) {
      const std::optional<char32_t> next = ahead(n);
      if (!next)
        return false;
      if (tables::is_cased(*next))
        return true;
      if (!tables::is_case_ignorable(*next))
        return false;
    }
  };
  // More_Above: followed by something of class 230, with nothing of class 0
  // or 230 between.
  const auto more_above = [&] {
    for (std::size_t n = 0;; ++n) {
      const std::optional<char32_t> next = ahead(n);
      if (!next)
        return false;
      const std::uint8_t cls = combining(*next);
      if (cls == 230)
        return true;
      if (cls == 0)
        return false;
    }
  };
  // Before_Dot: followed by U+0307, with nothing of class 0 or 230 between.
  const auto before_dot = [&] {
    for (std::size_t n = 0;; ++n) {
      const std::optional<char32_t> next = ahead(n);
      if (!next)
        return false;
      if (*next == 0x0307)
        return true;
      const std::uint8_t cls = combining(*next);
      if (cls == 0 || cls == 230)
        return false;
    }
  };

  if (which == casing::lower) {
    if (code_point == 0x03A3) {
      put({context.cased && !followed_by_cased() ? char32_t{0x03C2} : char32_t{0x03C3}});
      return;
    }
    if (language == casing_language::lithuanian) {
      if ((code_point == U'I' || code_point == U'J' || code_point == 0x012E) && more_above()) {
        put({tables::simple(casing::lower, code_point), 0x0307});
        return;
      }
      if (code_point == 0x00CC)
        return put({U'i', 0x0307, 0x0300});
      if (code_point == 0x00CD)
        return put({U'i', 0x0307, 0x0301});
      if (code_point == 0x0128)
        return put({U'i', 0x0307, 0x0303});
    }
    if (language == casing_language::turkic) {
      if (code_point == 0x0130)
        return put({U'i'});
      if (code_point == 0x0307 && context.capital_i)
        return;
      if (code_point == U'I' && !before_dot())
        return put({0x0131});
    }
  } else if (which == casing::upper || which == casing::title) {
    if (language == casing_language::lithuanian && code_point == 0x0307 &&
        context.soft_dotted)
      return;
    if (language == casing_language::turkic && code_point == U'i')
      return put({0x0130});
  } else if (language == casing_language::turkic) {
    const char32_t folded = tables::turkic_fold(code_point);
    if (folded != tables::simple(casing::fold, code_point))
      return put({folded});
  }
  const std::u32string_view full = tables::full(which, code_point);
  if (!full.empty())
    out.insert(out.end(), full.begin(), full.end());
  else
    out.push_back(tables::simple(which, code_point));
}

}  // namespace alef::detail::casing_rules

export namespace alef {

// Text in any UTF in lowercase, uppercase, or folded, as code points: each
// code point mapped as it is stepped to, with the text after it read ahead
// only as far as a condition asks. An input range, whatever the text is.
template <tables::casing Which, std::ranges::view V>
  requires utf_range<V>
class case_view : public std::ranges::view_interface<case_view<Which, V>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

  struct reading {
    I next;
    S last;
    casing_language language;
    std::vector<char32_t> ahead;  // read, and not mapped yet
    std::size_t ahead_at = 0;
    std::vector<char32_t> out;    // mapped, and being given out
    std::size_t given = 0;
    detail::casing_rules::before context;
    bool done = false;

    constexpr reading(I first, S end, casing_language in)
        : next(std::move(first)), last(std::move(end)), language(in) {}

    constexpr std::optional<char32_t> read() {
      if (next == last)
        return std::nullopt;
      bool well_formed = false;
      return detail::read<Unit>(next, last, well_formed);
    }
    constexpr std::optional<char32_t> take() {
      if (ahead_at < ahead.size())
        return ahead[ahead_at++];
      ahead.clear();
      ahead_at = 0;
      return read();
    }
    constexpr std::optional<char32_t> peek(std::size_t n) {
      while (ahead.size() - ahead_at <= n) {
        const std::optional<char32_t> code_point = read();
        if (!code_point)
          return std::nullopt;
        ahead.push_back(*code_point);
      }
      return ahead[ahead_at + n];
    }

    // What the next code point maps to -- or the next that maps to
    // anything -- into `out`.
    constexpr void map_next() {
      out.clear();
      given = 0;
      while (out.empty()) {
        const std::optional<char32_t> code_point = take();
        if (!code_point) {
          done = true;
          return;
        }
        detail::casing_rules::map(
            Which, *code_point, language, context,
            [this](std::size_t n) { return peek(n); }, out);
        context.take(*code_point);
      }
    }
  };

 public:
  class iterator {
   public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr explicit iterator(reading* at) noexcept : at_(at) {}

    constexpr char32_t operator*() const noexcept { return at_->out[at_->given]; }
    constexpr iterator& operator++() {
      if (++at_->given == at_->out.size())
        at_->map_next();
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) noexcept {
      return one.at_->done;
    }

   private:
    reading* at_ = nullptr;
  };

  case_view()
    requires std::default_initializable<V>
  = default;
  constexpr case_view(V base, casing_language language)
      : base_(std::move(base)), language_(language) {}

  constexpr iterator begin() {
    reading& at = reading_.emplace(std::ranges::begin(base_), std::ranges::end(base_),
                                   language_);
    at.map_next();
    return iterator(&at);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
  casing_language language_ = casing_language::other;
  detail::non_propagating<reading> reading_;
};

// Text in titlecase (the Unicode Standard, section 3.13, R3): at each word
// boundary of UAX #29, the first cased letter after it in titlecase, and what
// follows it to the next boundary in lowercase. The words are pieces of the
// text, so the text is a forward range; given out as code points, a word at
// a time.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class title_view : public std::ranges::view_interface<title_view<V>> {
  using Unit = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;
  using pieces = decltype(std::declval<V&>() | words);

  struct reading {
    pieces all;
    std::ranges::iterator_t<pieces> piece;
    S last;
    casing_language language;
    std::vector<char32_t> out;
    std::size_t given = 0;
    detail::casing_rules::before context;
    bool done = false;

    constexpr reading(V& text, casing_language in)
        : all(text | words), piece(std::ranges::begin(all)),
          last(std::ranges::end(text)), language(in) {}

    constexpr void map_next() {
      out.clear();
      given = 0;
      while (out.empty()) {
        if (piece == std::ranges::end(all)) {
          done = true;
          return;
        }
        const auto word = *piece;
        ++piece;
        bool titled = false;
        for (I at = word.begin(); at != word.end();) {
          bool well_formed = false;
          const char32_t code_point = detail::read<Unit>(at, last, well_formed);
          // The code points after this one, read from the text as far as a
          // condition asks.
          const auto ahead = [&, from = at](std::size_t n) {
            I reading_at = from;
            std::optional<char32_t> found;
            for (std::size_t k = 0; k <= n; ++k) {
              if (reading_at == last)
                return std::optional<char32_t>{};
              bool fine = false;
              found = detail::read<Unit>(reading_at, last, fine);
            }
            return found;
          };
          if (titled) {
            detail::casing_rules::map(tables::casing::lower, code_point, language,
                                      context, ahead, out);
          } else if (tables::is_cased(code_point)) {
            detail::casing_rules::map(tables::casing::title, code_point, language,
                                      context, ahead, out);
            titled = true;
          } else {
            out.push_back(code_point);
          }
          context.take(code_point);
        }
      }
    }
  };

 public:
  class iterator {
   public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    constexpr explicit iterator(reading* at) noexcept : at_(at) {}

    constexpr char32_t operator*() const noexcept { return at_->out[at_->given]; }
    constexpr iterator& operator++() {
      if (++at_->given == at_->out.size())
        at_->map_next();
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) noexcept {
      return one.at_->done;
    }

   private:
    reading* at_ = nullptr;
  };

  title_view()
    requires std::default_initializable<V>
  = default;
  constexpr title_view(V base, casing_language language)
      : base_(std::move(base)), language_(language) {}

  constexpr iterator begin() {
    reading& at = reading_.emplace(base_, language_);
    at.map_next();
    return iterator(&at);
  }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
  casing_language language_ = casing_language::other;
  detail::non_propagating<reading> reading_;
};

// text | as_lower, as_upper, as_title, as_folded; and with a language,
// text | as_lower(casing_language::turkic).
template <tables::casing Which>
struct case_fn : std::ranges::range_adaptor_closure<case_fn<Which>> {
  casing_language language = casing_language::other;

  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr auto operator()(Range&& range) const {
    using View = detail::all_of_t<Range>;
    if constexpr (Which == tables::casing::title)
      return title_view<View>(detail::all_of(std::forward<Range>(range)), language);
    else
      return case_view<Which, View>(detail::all_of(std::forward<Range>(range)), language);
  }
  constexpr case_fn operator()(casing_language in) const noexcept {
    case_fn with;
    with.language = in;
    return with;
  }
};
inline constexpr case_fn<tables::casing::lower> as_lower{};
inline constexpr case_fn<tables::casing::upper> as_upper{};
inline constexpr case_fn<tables::casing::title> as_title{};
inline constexpr case_fn<tables::casing::fold> as_folded{};

// Whether two texts, in any UTFs, are the same but for case: the same once
// both are folded (the Unicode Standard, D144, a default caseless match).
template <std::ranges::viewable_range One, std::ranges::viewable_range Other>
  requires utf_range<One> && utf_range<Other>
constexpr bool equal_ignoring_case(One&& one, Other&& other,
                                   casing_language language = casing_language::other) {
  return std::ranges::equal(std::forward<One>(one) | as_folded(language),
                            std::forward<Other>(other) | as_folded(language));
}

}  // namespace alef
