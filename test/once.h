// Text that can be read only once: an input range and nothing more, as a
// stream is -- and unlike a stream, one the compiler can read. In any UTF.
//
// Included by a file that imports std.
#pragma once

template <class Unit>
class once : public std::ranges::view_interface<once<Unit>> {
 public:
  class iterator {
   public:
    using value_type = Unit;
    using difference_type = std::ptrdiff_t;

    constexpr iterator(const Unit* at, const Unit* last)
        : at_(at), last_(last) {}
    iterator(iterator&&) = default;
    iterator& operator=(iterator&&) = default;

    constexpr Unit operator*() const { return *at_; }
    constexpr iterator& operator++() {
      ++at_;
      return *this;
    }
    constexpr void operator++(int) { ++at_; }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) {
      return one.at_ == one.last_;
    }

   private:
    const Unit* at_;
    const Unit* last_;
  };

  constexpr explicit once(std::basic_string_view<Unit> text) : text_(text) {}
  constexpr iterator begin() {
    return {text_.data(), text_.data() + text_.size()};
  }
  constexpr std::default_sentinel_t end() { return {}; }

 private:
  std::basic_string_view<Unit> text_;
};

template <class Unit, std::size_t Size>
once(const Unit (&)[Size]) -> once<Unit>;
