// Line breaking: UAX #14, the Unicode Line Breaking Algorithm, for Unicode
// 18.0.0, with the properties from the table of them.
//
// What comes apart here are the pieces between break opportunities -- where
// a line may end, and where it must -- each with the spaces after it.
export module alef.line;

import std;
import alef.utf;
import alef.tables.line;

export namespace alef {

using alef::line_break;

constexpr line_break line_break_of(char32_t code_point) noexcept {
  return tables::line_properties_of(code_point).line_break();
}

// A piece of text between break opportunities, and whether a line has to end
// after it: it ends in a line break of its own, BK, CR, LF or NL.
template <class I>
struct line_piece {
  std::ranges::subrange<I> text;
  bool mandatory = false;
};

}  // namespace alef

namespace alef::detail::line_rules {

using lb = line_break;

constexpr bool in(lb one, std::initializer_list<lb> all) noexcept {
  for (const lb each : all)
    if (one == each)
      return true;
  return false;
}

// A code point as the rules see it: its class, as LB1 resolves it, and what
// else they ask of it.
struct base {
  lb cls = lb::al;
  bool east_asian = false;
  bool pictographic_unassigned = false;
  bool initial_quote = false;  // QU and Pi
  bool final_quote = false;    // QU and Pf
  bool dotted_circle = false;  // U+25CC, which LB28a names
  bool opens_quote = false;    // LB15a: an initial quote where a quotation opens
};

// LB1: AI, SG and XX as AL; SA as CM where it is Mn or Mc and as AL where
// not; CJ as NS.
constexpr base resolve(char32_t code_point) noexcept {
  const tables::line_properties properties = tables::line_properties_of(code_point);
  lb cls = properties.line_break();
  if (in(cls, {lb::ai, lb::sg, lb::xx}))
    cls = lb::al;
  else if (cls == lb::sa)
    cls = properties.combining_mark() ? lb::cm : lb::al;
  else if (cls == lb::cj)
    cls = lb::ns;
  return {cls,
          properties.east_asian(),
          properties.pictographic() && properties.unassigned(),
          cls == lb::qu && properties.initial_punctuation(),
          cls == lb::qu && properties.final_punctuation(),
          code_point == 0x25CC,
          false};
}

// AK, the dotted circle or AS: an aksara of LB28a.
constexpr bool aksara(const base& one) noexcept {
  return one.cls == lb::ak || one.dotted_circle || one.cls == lb::as;
}

// LB25: digits so far, NU (SY | IS)*, and closed by CL or CP after them.
enum class number : std::uint8_t { none, digits, closed };

// What the rules ask of the text before a place.
struct context {
  bool start = true;
  // The class of the code point just before, after LB1 and before LB9 and
  // LB10.
  lb raw = lb::xx;
  // The last base, LB9 having joined combining marks and ZWJ to theirs, and
  // the one before it.
  std::optional<base> last;
  std::optional<base> before_last;
  // Where the last base is a space: the base before the spaces, and whether
  // there was none. X of the rules X SP* × Y.
  std::optional<base> before_spaces;
  number digits = number::none;
  std::size_t indicators = 0;

  constexpr const base* left() const noexcept {
    if (last && last->cls == lb::sp)
      return before_spaces ? &*before_spaces : nullptr;
    return last ? &*last : nullptr;
  }

  // A code point read after the place, the decision before it made.
  constexpr void take(const base& read) noexcept {
    if (!start && (read.cls == lb::cm || read.cls == lb::zwj) &&
        !in(raw, {lb::bk, lb::cr, lb::lf, lb::nl, lb::sp, lb::zw})) {
      raw = read.cls;  // LB9: joined to the base before it
      return;
    }
    base one = read;
    if (one.cls == lb::cm || one.cls == lb::zwj)
      one = base{};  // LB10: as U+0041, AL
    raw = read.cls;
    one.opens_quote =
        one.initial_quote &&
        (!last || in(last->cls, {lb::bk, lb::cr, lb::lf, lb::nl, lb::op, lb::qu,
                                 lb::gl, lb::sp, lb::zw}));
    if (one.cls == lb::sp && !(last && last->cls == lb::sp))
      before_spaces = last;
    if (one.cls == lb::nu)
      digits = number::digits;
    else if (digits == number::digits && in(one.cls, {lb::sy, lb::is}))
      digits = number::digits;
    else if (digits == number::digits && in(one.cls, {lb::cl, lb::cp}))
      digits = number::closed;
    else
      digits = number::none;
    indicators = one.cls == lb::ri ? indicators + 1 : 0;
    before_last = last;
    last = one;
    start = false;
  }
};

enum class decision : std::uint8_t { join, allowed, mandatory };

// What is between the text `before` and the code point `read` after it.
// `ahead(n)` is the n-th base after `read`, or nothing past the end, asked
// only where LB15b, LB15c, LB19a, LB25 or LB28a need it.
template <class Ahead>
constexpr decision between(const context& before, const base& read, Ahead&& ahead) {
  using enum decision;
  const lb raw = before.raw;
  lb cls = read.cls;
  if (raw == lb::bk)
    return mandatory;  // LB4
  if (raw == lb::cr && cls == lb::lf)
    return join;  // LB5
  if (in(raw, {lb::cr, lb::lf, lb::nl}))
    return mandatory;  // LB5
  if (in(cls, {lb::bk, lb::cr, lb::lf, lb::nl}))
    return join;  // LB6
  if (cls == lb::sp || cls == lb::zw)
    return join;  // LB7
  const base* x = before.left();
  if (x && x->cls == lb::zw)
    return allowed;  // LB8
  if (raw == lb::zwj)
    return join;  // LB8a
  if ((cls == lb::cm || cls == lb::zwj) &&
      !in(raw, {lb::bk, lb::cr, lb::lf, lb::nl, lb::sp, lb::zw}))
    return join;  // LB9
  base r = read;
  if (cls == lb::cm || cls == lb::zwj) {
    r = base{};  // LB10
    cls = lb::al;
  }
  const base& l = *before.last;
  const std::optional<base>& ll = before.before_last;
  if (cls == lb::wj || l.cls == lb::wj)
    return join;  // LB11
  if (l.cls == lb::gl)
    return join;  // LB12
  if (cls == lb::gl && !in(l.cls, {lb::sp, lb::hy, lb::hh}))
    return join;  // LB12a
  if (in(cls, {lb::cl, lb::cp, lb::ex, lb::sy}))
    return join;  // LB13
  if (x && x->cls == lb::op)
    return join;  // LB14
  if (x && x->opens_quote)
    return join;  // LB15a
  if (r.final_quote) {
    const std::optional<base> next = ahead(0);
    if (!next || in(next->cls, {lb::sp, lb::gl, lb::wj, lb::cl, lb::qu, lb::cp, lb::ex,
                                lb::is, lb::sy, lb::bk, lb::cr, lb::lf, lb::nl, lb::zw}))
      return join;  // LB15b
  }
  if (l.cls == lb::sp && cls == lb::is) {
    const std::optional<base> next = ahead(0);
    if (next && next->cls == lb::nu)
      return allowed;  // LB15c
  }
  if (cls == lb::is)
    return join;  // LB15d
  if (x && in(x->cls, {lb::cl, lb::cp}) && cls == lb::ns)
    return join;  // LB16
  if (x && x->cls == lb::b2 && cls == lb::b2)
    return join;  // LB17
  if (l.cls == lb::sp)
    return allowed;  // LB18
  if (cls == lb::qu && !r.initial_quote)
    return join;  // LB19
  if (l.cls == lb::qu && !l.final_quote)
    return join;  // LB19
  if (cls == lb::qu) {
    if (!l.east_asian)
      return join;  // LB19a
    const std::optional<base> next = ahead(0);
    if (!next || !next->east_asian)
      return join;  // LB19a
  }
  if (l.cls == lb::qu && (!r.east_asian || !ll || !ll->east_asian))
    return join;  // LB19a
  if (cls == lb::cb || l.cls == lb::cb)
    return allowed;  // LB20
  if (in(l.cls, {lb::hy, lb::hh}) && in(cls, {lb::al, lb::hl}) &&
      (!ll || in(ll->cls, {lb::bk, lb::cr, lb::lf, lb::nl, lb::sp, lb::zw, lb::cb, lb::gl})))
    return join;  // LB20a
  if (in(cls, {lb::ba, lb::hh, lb::hy, lb::ns}) || l.cls == lb::bb)
    return join;  // LB21
  if (ll && ll->cls == lb::hl && in(l.cls, {lb::hy, lb::hh}) && cls != lb::hl)
    return join;  // LB21a
  if (l.cls == lb::sy && cls == lb::hl)
    return join;  // LB21b
  if (cls == lb::in)
    return join;  // LB22
  if ((in(l.cls, {lb::al, lb::hl}) && cls == lb::nu) ||
      (l.cls == lb::nu && in(cls, {lb::al, lb::hl})))
    return join;  // LB23
  if ((l.cls == lb::pr && in(cls, {lb::id, lb::eb, lb::em})) ||
      (in(l.cls, {lb::id, lb::eb, lb::em}) && cls == lb::po))
    return join;  // LB23a
  if ((in(l.cls, {lb::pr, lb::po}) && in(cls, {lb::al, lb::hl})) ||
      (in(l.cls, {lb::al, lb::hl}) && in(cls, {lb::pr, lb::po})))
    return join;  // LB24
  if (before.digits == number::closed && in(cls, {lb::po, lb::pr}))
    return join;  // LB25
  if (before.digits == number::digits && in(cls, {lb::po, lb::pr, lb::nu}))
    return join;  // LB25
  if (in(l.cls, {lb::po, lb::pr}) && cls == lb::op) {
    const std::optional<base> next = ahead(0);
    if (next && next->cls == lb::nu)
      return join;  // LB25
    if (next && next->cls == lb::is) {
      const std::optional<base> after = ahead(1);
      if (after && after->cls == lb::nu)
        return join;  // LB25
    }
  }
  if (in(l.cls, {lb::po, lb::pr, lb::hy, lb::is}) && cls == lb::nu)
    return join;  // LB25
  if (l.cls == lb::jl && in(cls, {lb::jl, lb::jv, lb::h2, lb::h3}))
    return join;  // LB26
  if (in(l.cls, {lb::jv, lb::h2}) && in(cls, {lb::jv, lb::jt}))
    return join;  // LB26
  if (in(l.cls, {lb::jt, lb::h3}) && cls == lb::jt)
    return join;  // LB26
  if (in(l.cls, {lb::jl, lb::jv, lb::jt, lb::h2, lb::h3}) && cls == lb::po)
    return join;  // LB27
  if (l.cls == lb::pr && in(cls, {lb::jl, lb::jv, lb::jt, lb::h2, lb::h3}))
    return join;  // LB27
  if (in(l.cls, {lb::al, lb::hl}) && in(cls, {lb::al, lb::hl}))
    return join;  // LB28
  if (l.cls == lb::ap && aksara(r))
    return join;  // LB28a
  if (aksara(l) && in(cls, {lb::vf, lb::vi}))
    return join;  // LB28a
  if (ll && aksara(*ll) && l.cls == lb::vi && (cls == lb::ak || r.dotted_circle))
    return join;  // LB28a
  if (aksara(l) && aksara(r)) {
    const std::optional<base> next = ahead(0);
    if (next && next->cls == lb::vf)
      return join;  // LB28a
  }
  if (l.cls == lb::is && in(cls, {lb::al, lb::hl}))
    return join;  // LB29
  if (in(l.cls, {lb::al, lb::hl, lb::nu}) && cls == lb::op && !r.east_asian)
    return join;  // LB30
  if (l.cls == lb::cp && !l.east_asian && in(cls, {lb::al, lb::hl, lb::nu}))
    return join;  // LB30
  if (l.cls == lb::ri && cls == lb::ri)
    return before.indicators % 2 == 1 ? join : allowed;  // LB30a
  if (cls == lb::em && (l.cls == lb::eb || l.pictographic_unassigned))
    return join;  // LB30b
  return allowed;  // LB31
}

// The n-th base from `at`, passing over what LB9 joins to the one before it.
template <code_unit Unit, class I, class S>
constexpr std::optional<base> base_ahead(I at, const S& last, std::size_t n) {
  while (at != last) {
    bool well_formed = false;
    const base one = resolve(read<Unit>(at, last, well_formed));
    if (one.cls == lb::cm || one.cls == lb::zwj)
      continue;
    if (n == 0)
      return one;
    --n;
  }
  return std::nullopt;
}

// The end of the piece that begins at `from`, and whether the break there is
// one the text asks for. `state` is what the rules know of the text before
// `from`, and is carried on to the end of the piece.
template <code_unit Unit, class I, class S>
constexpr std::pair<I, bool> next_break(context& state, I from, const S& last) {
  I at = from;
  if (at == last)
    return {at, false};
  bool well_formed = false;
  state.take(resolve(read<Unit>(at, last, well_formed)));
  while (at != last) {
    I here = at;
    const base next = resolve(read<Unit>(at, last, well_formed));
    const decision what = between(
        state, next, [&](std::size_t n) { return base_ahead<Unit>(at, last, n); });
    if (what != decision::join)
      return {here, what == decision::mandatory};
    state.take(next);
  }
  return {at, false};
}

}  // namespace alef::detail::line_rules

export namespace alef {

// The pieces between line break opportunities of text in any UTF, each a
// piece of the text with the spaces after it, and whether a line has to end
// after it. What the rules know of the text is carried from its start, so
// the view goes forwards.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class line_view : public std::ranges::view_interface<line_view<V>> {
  using From = detail::unit_of<V>;
  using I = std::ranges::iterator_t<V>;
  using S = std::ranges::sentinel_t<V>;

 public:
  class iterator {
   public:
    using value_type = line_piece<I>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    iterator()
      requires std::default_initializable<I>
    = default;
    constexpr iterator(I first, S last) : begin_(std::move(first)), last_(std::move(last)) {
      step();
    }

    constexpr line_piece<I> operator*() const { return {{begin_, end_}, mandatory_}; }
    constexpr iterator& operator++() {
      begin_ = end_;
      step();
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator was = *this;
      ++*this;
      return was;
    }
    friend constexpr bool operator==(const iterator& one, const iterator& other) {
      return one.begin_ == other.begin_;
    }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) {
      return one.begin_ == one.last_;
    }

   private:
    constexpr void step() {
      const auto [end, mandatory] =
          detail::line_rules::next_break<From>(state_, begin_, last_);
      end_ = end;
      mandatory_ = mandatory;
    }

    I begin_{};
    I end_{};
    [[no_unique_address]] S last_{};
    bool mandatory_ = false;
    detail::line_rules::context state_;
  };

  line_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit line_view(V base) : base_(std::move(base)) {}

  constexpr iterator begin() { return {std::ranges::begin(base_), std::ranges::end(base_)}; }
  constexpr std::default_sentinel_t end() const noexcept { return {}; }

 private:
  V base_ = V();
};

// text | line_breaks, or line_breaks(text).
struct line_breaks_fn : std::ranges::range_adaptor_closure<line_breaks_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range> && std::ranges::forward_range<detail::all_of_t<Range>>
  constexpr auto operator()(Range&& range) const {
    return line_view<detail::all_of_t<Range>>(detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr line_breaks_fn line_breaks{};

}  // namespace alef
