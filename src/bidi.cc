// The bidirectional algorithm: UAX #9, the Unicode Bidirectional Algorithm,
// for Unicode 18.0.0, with the properties from the table of them.
//
// Text is split into paragraphs by P1, each ending after its paragraph
// separator, and a paragraph of text in any UTF is given its embedding
// levels -- its own by
// P2 and P3, explicit embeddings, overrides and isolates by X1 to X10, and
// the rest by W1 to W7, N0 to N2, I1 and I2 -- and a line of it its levels
// by L1 and its visual order by L2.
export module alef.bidi;

import std;
import alef.utf;
import alef.tables.bidi;

export namespace alef {

using alef::bidi_class;

constexpr bidi_class bidi_class_of(char32_t code_point) noexcept {
  return tables::bidi_properties_of(code_point).cls();
}

// The glyph to show for a code point in right-to-left text
// (Bidi_Mirroring_Glyph), or itself where it has none.
constexpr char32_t mirrored(char32_t code_point) noexcept {
  const char32_t glyph = tables::mirroring_glyph(code_point);
  return glyph != 0 ? glyph : code_point;
}

// The direction a paragraph goes: as asked, or as its first strong
// character says (P2, P3), left to right where it has none.
enum class bidi_direction : std::uint8_t { ltr, rtl, automatic };

// The level of what rule X9 removes: embeddings, overrides, the end of them,
// and boundary neutrals.
inline constexpr std::uint8_t removed_level = 0xFF;

}  // namespace alef

namespace alef::detail::bidi_rules {

using bc = bidi_class;
inline constexpr std::size_t none = static_cast<std::size_t>(-1);

constexpr bool in(bc one, std::initializer_list<bc> all) noexcept {
  for (const bc each : all)
    if (one == each)
      return true;
  return false;
}
constexpr bool removed_by_x9(bc one) noexcept {
  return in(one, {bc::lre, bc::rle, bc::lro, bc::rlo, bc::pdf, bc::bn});
}
constexpr bool isolate_initiator(bc one) noexcept {
  return in(one, {bc::lri, bc::rli, bc::fsi});
}

// BD9: the PDI matching each isolate initiator, and whether each PDI matches
// one.
struct isolates {
  std::vector<std::size_t> matching_pdi;
  std::vector<bool> matched;
};

constexpr isolates match_isolates(std::span<const bc> types) {
  isolates out{std::vector<std::size_t>(types.size(), none),
               std::vector<bool>(types.size(), false)};
  std::vector<std::size_t> open;
  for (std::size_t at = 0; at < types.size(); ++at) {
    if (isolate_initiator(types[at])) {
      open.push_back(at);
    } else if (types[at] == bc::pdi && !open.empty()) {
      out.matching_pdi[open.back()] = at;
      out.matched[at] = true;
      open.pop_back();
    } else if (types[at] == bc::b) {
      open.clear();
    }
  }
  return out;
}

// P2, P3: the level the first strong character from `from` to `to` says,
// passing over isolates; nothing where there is none.
constexpr std::optional<std::uint8_t> first_strong(std::span<const bc> types,
                                                   const isolates& matched,
                                                   std::size_t from, std::size_t to) {
  for (std::size_t at = from; at < to; ++at) {
    const bc one = types[at];
    if (one == bc::l)
      return 0;
    if (one == bc::r || one == bc::al)
      return 1;
    if (one == bc::b)
      return std::nullopt;
    if (isolate_initiator(one)) {
      if (matched.matching_pdi[at] == none)
        return std::nullopt;
      at = matched.matching_pdi[at];
    }
  }
  return std::nullopt;
}

constexpr char32_t canonical_bracket(char32_t code_point) noexcept {
  if (code_point == 0x2329)
    return 0x3008;
  if (code_point == 0x232A)
    return 0x3009;
  return code_point;
}

struct resolved {
  std::uint8_t paragraph = 0;
  std::vector<std::uint8_t> levels;
};

// The levels of a paragraph: the classes of its characters, and their code
// points, where there are any to find paired brackets by (N0).
constexpr resolved resolve(std::span<const bc> original, std::span<const char32_t> code_points,
                           bidi_direction direction) {
  const std::size_t size = original.size();
  std::vector<bc> types(original.begin(), original.end());
  const isolates matched = match_isolates(types);
  const std::uint8_t paragraph =
      direction == bidi_direction::ltr   ? 0
      : direction == bidi_direction::rtl ? 1
                                         : first_strong(types, matched, 0, size).value_or(0);
  std::vector<std::uint8_t> levels(size, paragraph);

  // X1 to X8: the directional status stack.
  struct entry {
    std::uint8_t level = 0;
    std::uint8_t override_ = 0;  // 0 none, 1 left to right, 2 right to left
    bool isolate = false;
  };
  constexpr std::uint8_t max_depth = 125;
  std::vector<entry> stack{{paragraph, 0, false}};
  std::size_t overflow_isolates = 0;
  std::size_t overflow_embeddings = 0;
  std::size_t valid_isolates = 0;
  const auto next_odd = [](std::uint8_t level) { return static_cast<std::uint8_t>((level + 1) | 1); };
  const auto next_even = [](std::uint8_t level) { return static_cast<std::uint8_t>((level + 2) & ~1); };
  const auto overridden = [&](std::size_t at) {
    if (stack.back().override_ == 1)
      types[at] = bc::l;
    else if (stack.back().override_ == 2)
      types[at] = bc::r;
  };
  for (std::size_t at = 0; at < size; ++at) {
    const bc one = original[at];
    if (in(one, {bc::rle, bc::lre, bc::rlo, bc::lro})) {  // X2 to X5
      const bool rtl = one == bc::rle || one == bc::rlo;
      const std::uint8_t level = rtl ? next_odd(stack.back().level) : next_even(stack.back().level);
      levels[at] = stack.back().level;
      if (level <= max_depth && overflow_isolates == 0 && overflow_embeddings == 0)
        stack.push_back({level,
                         static_cast<std::uint8_t>(one == bc::rlo ? 2 : one == bc::lro ? 1 : 0),
                         false});
      else if (overflow_isolates == 0)
        ++overflow_embeddings;
    } else if (isolate_initiator(one)) {  // X5a to X5c
      bool rtl = one == bc::rli;
      if (one == bc::fsi) {
        const std::size_t end = matched.matching_pdi[at] == none ? size : matched.matching_pdi[at];
        rtl = first_strong(types, matched, at + 1, end).value_or(0) == 1;
      }
      levels[at] = stack.back().level;
      overridden(at);
      const std::uint8_t level = rtl ? next_odd(stack.back().level) : next_even(stack.back().level);
      if (level <= max_depth && overflow_isolates == 0 && overflow_embeddings == 0) {
        ++valid_isolates;
        stack.push_back({level, 0, true});
      } else {
        ++overflow_isolates;
      }
    } else if (one == bc::pdi) {  // X6a
      if (overflow_isolates > 0) {
        --overflow_isolates;
      } else if (valid_isolates > 0) {
        overflow_embeddings = 0;
        while (!stack.back().isolate)
          stack.pop_back();
        stack.pop_back();
        --valid_isolates;
      }
      levels[at] = stack.back().level;
      overridden(at);
    } else if (one == bc::pdf) {  // X7
      if (overflow_isolates > 0) {
      } else if (overflow_embeddings > 0) {
        --overflow_embeddings;
      } else if (!stack.back().isolate && stack.size() >= 2) {
        stack.pop_back();
      }
      levels[at] = stack.back().level;
    } else if (one == bc::b) {  // X8
      levels[at] = paragraph;
    } else if (one == bc::bn) {
      levels[at] = stack.back().level;
    } else {  // X6
      levels[at] = stack.back().level;
      overridden(at);
    }
  }

  // X9: what is removed, and X10: the level runs of what is kept, and the
  // isolating run sequences they make.
  std::vector<std::size_t> kept;
  std::vector<std::size_t> position(size, none);
  for (std::size_t at = 0; at < size; ++at)
    if (!removed_by_x9(original[at])) {
      position[at] = kept.size();
      kept.push_back(at);
    }
  // The levels the explicit rules gave: what the sos and eos of a sequence
  // are decided by, and not the levels I1 and I2 give the sequences next to
  // it, which are resolved first.
  const std::vector<std::uint8_t> explicit_levels = levels;
  std::vector<std::pair<std::size_t, std::size_t>> runs;
  std::vector<std::size_t> run_of(kept.size());
  for (std::size_t from = 0; from < kept.size();) {
    std::size_t to = from;
    while (to < kept.size() && levels[kept[to]] == levels[kept[from]])
      run_of[to++] = runs.size();
    runs.push_back({from, to});
    from = to;
  }

  for (const auto& start : runs) {
    const std::size_t first = kept[start.first];
    if (original[first] == bc::pdi && matched.matched[first])
      continue;
    std::vector<std::size_t> sequence;
    for (std::size_t run = run_of[start.first];;) {
      for (std::size_t at = runs[run].first; at < runs[run].second; ++at)
        sequence.push_back(kept[at]);
      const std::size_t last = kept[runs[run].second - 1];
      if (!isolate_initiator(original[last]) || matched.matching_pdi[last] == none)
        break;
      run = run_of[position[matched.matching_pdi[last]]];
    }

    const std::uint8_t level = levels[sequence.front()];
    const std::size_t front = position[sequence.front()];
    const std::size_t back = position[sequence.back()];
    const std::uint8_t before = front == 0 ? paragraph : explicit_levels[kept[front - 1]];
    const std::uint8_t after = isolate_initiator(original[sequence.back()]) || back + 1 == kept.size()
                                   ? paragraph
                                   : explicit_levels[kept[back + 1]];
    const bc sos = std::max(before, level) % 2 == 1 ? bc::r : bc::l;
    const bc eos = std::max(after, level) % 2 == 1 ? bc::r : bc::l;
    const bc embedding = level % 2 == 1 ? bc::r : bc::l;
    const std::size_t count = sequence.size();
    const auto type = [&](std::size_t k) -> bc& { return types[sequence[k]]; };

    std::vector<bc> before_w1(count);
    for (std::size_t k = 0; k < count; ++k)
      before_w1[k] = type(k);

    for (std::size_t k = 0; k < count; ++k)  // W1
      if (type(k) == bc::nsm)
        type(k) = k == 0 ? sos
                  : isolate_initiator(type(k - 1)) || type(k - 1) == bc::pdi ? bc::on
                                                                           : type(k - 1);
    bc strong = sos;
    for (std::size_t k = 0; k < count; ++k) {  // W2
      if (in(type(k), {bc::l, bc::r, bc::al}))
        strong = type(k);
      else if (type(k) == bc::en && strong == bc::al)
        type(k) = bc::an;
    }
    for (std::size_t k = 0; k < count; ++k)  // W3
      if (type(k) == bc::al)
        type(k) = bc::r;
    for (std::size_t k = 1; k + 1 < count; ++k) {  // W4
      if (type(k) == bc::es && type(k - 1) == bc::en && type(k + 1) == bc::en)
        type(k) = bc::en;
      else if (type(k) == bc::cs && type(k - 1) == bc::en && type(k + 1) == bc::en)
        type(k) = bc::en;
      else if (type(k) == bc::cs && type(k - 1) == bc::an && type(k + 1) == bc::an)
        type(k) = bc::an;
    }
    for (std::size_t k = 0; k < count;) {  // W5
      if (type(k) != bc::et) {
        ++k;
        continue;
      }
      std::size_t end = k;
      while (end < count && type(end) == bc::et)
        ++end;
      if ((k > 0 && type(k - 1) == bc::en) || (end < count && type(end) == bc::en))
        for (std::size_t each = k; each < end; ++each)
          type(each) = bc::en;
      k = end;
    }
    for (std::size_t k = 0; k < count; ++k)  // W6
      if (in(type(k), {bc::es, bc::et, bc::cs}))
        type(k) = bc::on;
    strong = sos;
    for (std::size_t k = 0; k < count; ++k) {  // W7
      if (type(k) == bc::l || type(k) == bc::r)
        strong = type(k);
      else if (type(k) == bc::en && strong == bc::l)
        type(k) = bc::l;
    }

    if (!code_points.empty()) {  // N0: paired brackets (BD16)
      struct opening {
        char32_t pair;
        std::size_t k;
      };
      std::vector<opening> open;
      std::vector<std::pair<std::size_t, std::size_t>> pairs;
      for (std::size_t k = 0; k < count; ++k) {
        if (type(k) != bc::on)
          continue;
        const char32_t code_point = code_points[sequence[k]];
        const tables::bidi_properties properties = tables::bidi_properties_of(code_point);
        if (properties.opening_bracket()) {
          if (open.size() == 63)
            break;
          open.push_back({canonical_bracket(tables::paired_bracket(code_point)), k});
        } else if (properties.closing_bracket()) {
          const char32_t closing = canonical_bracket(code_point);
          for (std::size_t depth = open.size(); depth-- > 0;)
            if (open[depth].pair == closing) {
              pairs.push_back({open[depth].k, k});
              open.resize(depth);
              break;
            }
        }
      }
      std::ranges::sort(pairs);
      const auto strong_of = [](bc one) -> std::optional<bc> {
        if (one == bc::l)
          return bc::l;
        if (in(one, {bc::r, bc::en, bc::an}))
          return bc::r;
        return std::nullopt;
      };
      const bc opposite = embedding == bc::l ? bc::r : bc::l;
      for (const auto& [opened, closed] : pairs) {
        bool same = false;
        bool other = false;
        for (std::size_t k = opened + 1; k < closed; ++k)
          if (const auto found = strong_of(type(k)))
            (*found == embedding ? same : other) = true;
        std::optional<bc> set;
        if (same) {
          set = embedding;
        } else if (other) {
          bc context = sos;
          for (std::size_t k = opened; k-- > 0;)
            if (const auto found = strong_of(type(k))) {
              context = *found;
              break;
            }
          set = context == opposite ? opposite : embedding;
        }
        if (set) {
          type(opened) = *set;
          type(closed) = *set;
          for (std::size_t k = opened + 1; k < count && before_w1[k] == bc::nsm; ++k)
            type(k) = *set;
          for (std::size_t k = closed + 1; k < count && before_w1[k] == bc::nsm; ++k)
            type(k) = *set;
        }
      }
    }

    const auto neutral = [](bc one) {
      return in(one, {bc::b, bc::s, bc::ws, bc::on, bc::lri, bc::rli, bc::fsi, bc::pdi});
    };
    const auto direction_of = [](bc one) { return one == bc::l ? bc::l : bc::r; };
    for (std::size_t k = 0; k < count;) {  // N1, N2
      if (!neutral(type(k))) {
        ++k;
        continue;
      }
      std::size_t end = k;
      while (end < count && neutral(type(end)))
        ++end;
      const bc left = k == 0 ? sos : direction_of(type(k - 1));
      const bc right = end == count ? eos : direction_of(type(end));
      const bc set = left == right ? left : embedding;
      for (std::size_t each = k; each < end; ++each)
        type(each) = set;
      k = end;
    }

    for (std::size_t k = 0; k < count; ++k) {  // I1, I2
      std::uint8_t& at = levels[sequence[k]];
      if (at % 2 == 0) {
        if (type(k) == bc::r)
          at += 1;
        else if (type(k) == bc::an || type(k) == bc::en)
          at += 2;
      } else if (type(k) == bc::l || type(k) == bc::en || type(k) == bc::an) {
        at += 1;
      }
    }
  }

  for (std::size_t at = 0; at < size; ++at)
    if (removed_by_x9(original[at]))
      levels[at] = removed_level;
  return {paragraph, std::move(levels)};
}

}  // namespace alef::detail::bidi_rules

export namespace alef {

// A paragraph, its embedding levels resolved: of text in any UTF, or of
// bidi classes, as the tests of UAX #9 give them.
class bidi_paragraph {
 public:
  template <std::ranges::viewable_range Range>
    requires utf_range<Range>
  constexpr explicit bidi_paragraph(Range&& text,
                                    bidi_direction direction = bidi_direction::automatic) {
    std::vector<char32_t> code_points;
    for (const char32_t code_point : std::forward<Range>(text) | as_utf32) {
      code_points.push_back(code_point);
      classes_.push_back(bidi_class_of(code_point));
    }
    take(detail::bidi_rules::resolve(classes_, code_points, direction));
  }

  constexpr bidi_paragraph(std::span<const bidi_class> classes,
                           bidi_direction direction = bidi_direction::automatic)
      : classes_(classes.begin(), classes.end()) {
    take(detail::bidi_rules::resolve(classes_, {}, direction));
  }

  // Its own level, 0 or 1.
  constexpr std::uint8_t level() const noexcept { return level_; }
  // The level of each code point, before L1; removed_level where X9 removes it.
  constexpr std::span<const std::uint8_t> levels() const noexcept { return levels_; }
  constexpr std::size_t size() const noexcept { return levels_.size(); }

  // The levels of a line of it, the code points from `first` to `last`, by
  // L1: separators, and the spaces and isolates before them and at the end
  // of the line, at the level of the paragraph.
  constexpr std::vector<std::uint8_t> line_levels(std::size_t first, std::size_t last) const {
    using bc = bidi_class;
    std::vector<std::uint8_t> out(levels_.begin() + first, levels_.begin() + last);
    bool trailing = true;
    for (std::size_t k = out.size(); k-- > 0;) {
      const bc one = classes_[first + k];
      if (one == bc::s || one == bc::b) {
        out[k] = level_;
        trailing = true;
      } else if (detail::bidi_rules::in(one, {bc::ws, bc::fsi, bc::lri, bc::rli, bc::pdi})) {
        if (trailing)
          out[k] = level_;
      } else if (!detail::bidi_rules::removed_by_x9(one)) {
        trailing = false;
      }
    }
    return out;
  }

  // The visual order of a line of it, left to right, by L2: the positions of
  // its code points, but those X9 removes.
  constexpr std::vector<std::size_t> visual_order(std::size_t first, std::size_t last) const {
    const std::vector<std::uint8_t> line = line_levels(first, last);
    std::vector<std::size_t> order;
    std::vector<std::uint8_t> at;
    for (std::size_t k = 0; k < line.size(); ++k)
      if (line[k] != removed_level) {
        order.push_back(first + k);
        at.push_back(line[k]);
      }
    if (at.empty())
      return order;
    const std::uint8_t highest = std::ranges::max(at);
    const std::uint8_t lowest = std::ranges::min(at);
    const std::uint8_t lowest_odd = lowest % 2 == 1 ? lowest : static_cast<std::uint8_t>(lowest + 1);
    for (int level = highest; level >= lowest_odd; --level)
      for (std::size_t from = 0; from < at.size();) {
        if (at[from] < level) {
          ++from;
          continue;
        }
        std::size_t to = from;
        while (to < at.size() && at[to] >= level)
          ++to;
        std::reverse(order.begin() + from, order.begin() + to);
        std::reverse(at.begin() + from, at.begin() + to);
        from = to;
      }
    return order;
  }

 private:
  constexpr void take(detail::bidi_rules::resolved done) {
    level_ = done.paragraph;
    levels_ = std::move(done.levels);
  }

  std::vector<bidi_class> classes_;
  std::uint8_t level_ = 0;
  std::vector<std::uint8_t> levels_;
};

}  // namespace alef

namespace alef::detail::bidi_rules {

// Whether a code point separates paragraphs: Bidi_Class B.
constexpr bool separates(char32_t code_point) noexcept {
  return bidi_class_of(code_point) == bidi_class::b;
}

template <code_unit Unit, class I>
constexpr char32_t code_point_at(I at, const I& end) {
  bool well_formed = false;
  return read<Unit>(at, end, well_formed);
}

// P1: the end of the paragraph that begins at `at` -- just past its
// paragraph separator, a CR LF being one, or `last`.
template <code_unit Unit, class I, class S>
constexpr I paragraph_end(I at, const S& last) {
  bool well_formed = false;
  while (at != last) {
    const char32_t one = read<Unit>(at, last, well_formed);
    if (!separates(one))
      continue;
    if (one == U'\r' && at != last) {
      I after = at;
      if (read<Unit>(after, last, well_formed) == U'\n')
        at = after;
    }
    return at;
  }
  return at;
}

// The start of the paragraph that ends at `at`, where one begins: back over
// its separator, if it has one, and on to just after the separator before.
template <code_unit Unit, class I>
constexpr I paragraph_start(const I& first, I at) {
  if (at == first)
    return at;
  I back = step_back<Unit>(first, at);
  if (code_point_at<Unit>(back, at) == U'\n' && back != first) {
    I before = step_back<Unit>(first, back);
    if (code_point_at<Unit>(before, back) == U'\r')
      back = before;
  }
  at = back;
  while (at != first) {
    I before = step_back<Unit>(first, at);
    if (separates(code_point_at<Unit>(before, at)))
      break;
    at = before;
  }
  return at;
}

}  // namespace alef::detail::bidi_rules

export namespace alef {

// P1: the next paragraph boundary after `at`, which is one -- just past a
// paragraph separator (Bidi_Class B, a CR LF being one), or `last`.
template <std::forward_iterator I, std::sentinel_for<I> S>
  requires code_unit<std::iter_value_t<I>>
constexpr I next_paragraph_boundary(I at, S last) {
  return detail::bidi_rules::paragraph_end<std::iter_value_t<I>>(std::move(at), last);
}

// The paragraph boundary before `at`, which is one. `first` is where the
// text begins.
template <std::bidirectional_iterator I>
  requires code_unit<std::iter_value_t<I>>
constexpr I prev_paragraph_boundary(I first, I at) {
  return detail::bidi_rules::paragraph_start<std::iter_value_t<I>>(first, std::move(at));
}

// The paragraphs of text in any UTF by P1, each the part of the text it was
// read from with its separator at its end; bidirectional if the text is.
// Each is what a bidi_paragraph is made of.
template <std::ranges::view V>
  requires utf_range<V> && std::ranges::forward_range<V>
class paragraph_view : public std::ranges::view_interface<paragraph_view<V>> {
  using From = detail::unit_of<V>;

  template <bool Const>
  class iterator {
    using Base = std::conditional_t<Const, const V, V>;
    using I = std::ranges::iterator_t<Base>;
    using S = std::ranges::sentinel_t<Base>;
    static constexpr bool bidirectional = std::ranges::bidirectional_range<Base>;

   public:
    using value_type = std::ranges::subrange<I>;
    using difference_type = std::ptrdiff_t;
    using iterator_concept =
        std::conditional_t<bidirectional, std::bidirectional_iterator_tag,
                           std::forward_iterator_tag>;

    iterator()
      requires std::default_initializable<I>
    = default;
    constexpr iterator(Base& base, I at)
        : begin_(std::move(at)), last_(std::ranges::end(base)) {
      if constexpr (bidirectional)
        first_ = std::ranges::begin(base);
      end_ = detail::bidi_rules::paragraph_end<From>(begin_, last_);
    }

    constexpr std::ranges::subrange<I> operator*() const { return {begin_, end_}; }

    constexpr iterator& operator++() {
      begin_ = end_;
      end_ = detail::bidi_rules::paragraph_end<From>(begin_, last_);
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator was = *this;
      ++*this;
      return was;
    }
    constexpr iterator& operator--()
      requires bidirectional
    {
      end_ = begin_;
      begin_ = detail::bidi_rules::paragraph_start<From>(first_, begin_);
      return *this;
    }
    constexpr iterator operator--(int)
      requires bidirectional
    {
      iterator was = *this;
      --*this;
      return was;
    }

    friend constexpr bool operator==(const iterator& one, const iterator& other) {
      return one.begin_ == other.begin_;
    }
    friend constexpr bool operator==(const iterator& one, std::default_sentinel_t) {
      return one.begin_ == one.last_;
    }

    // Where the paragraph begins in V.
    constexpr I base() const { return begin_; }

   private:
    [[no_unique_address]] std::conditional_t<bidirectional, I, detail::nothing>
        first_{};
    I begin_{};
    I end_{};
    [[no_unique_address]] S last_{};
  };

 public:
  paragraph_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit paragraph_view(V base) : base_(std::move(base)) {}

  constexpr V base() const&
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }

  constexpr auto begin() { return iterator<false>(base_, std::ranges::begin(base_)); }
  constexpr auto begin() const
    requires std::ranges::forward_range<const V>
  {
    return iterator<true>(base_, std::ranges::begin(base_));
  }
  constexpr auto end() {
    if constexpr (std::ranges::common_range<V>)
      return iterator<false>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }
  constexpr auto end() const
    requires std::ranges::forward_range<const V>
  {
    if constexpr (std::ranges::common_range<const V>)
      return iterator<true>(base_, std::ranges::end(base_));
    else
      return std::default_sentinel;
  }

 private:
  V base_ = V();
};

// text | paragraphs, or paragraphs(text).
struct paragraphs_fn : std::ranges::range_adaptor_closure<paragraphs_fn> {
  template <std::ranges::viewable_range Range>
    requires utf_range<Range> && std::ranges::forward_range<detail::all_of_t<Range>>
  constexpr auto operator()(Range&& range) const {
    return paragraph_view<detail::all_of_t<Range>>(
        detail::all_of(std::forward<Range>(range)));
  }
};
inline constexpr paragraphs_fn paragraphs{};

}  // namespace alef

template <class V>
inline constexpr bool std::ranges::enable_borrowed_range<alef::paragraph_view<V>> =
    std::ranges::enable_borrowed_range<V>;
