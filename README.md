# alef

Unicode for C++23: text in any of the UTFs, read lazily, and the Unicode
Character Database read as it is published.

Nothing is generated. `ucd/` holds files of the UCD 18.0.0 exactly as the
Unicode Consortium publishes them, `#embed` puts their bytes into the module,
and constexpr code reads them while the module interface is compiled. A new
version of Unicode is new files in `ucd/`.

Nothing is done ahead of time either. alef gives views: they read text as
they are asked for more of it, and write nothing anywhere to be looked at
later. All of it is constexpr.

## What there is

- **The UTFs.** `alef::as_utf8`, `alef::as_utf16` and `alef::as_utf32` read
  text in any of the three and give it in the one named, a code unit at a
  time. Which UTF text is in, the type of its code units says: `char` and
  `char8_t` are UTF-8, `char16_t` is UTF-16, `char32_t` is UTF-32. A view is
  as lazy as its text: an input range over text that can be read only once,
  forward over forward, bidirectional over bidirectional. What is not
  well-formed reads as U+FFFD (in UTF-8, one for each maximal subpart of an
  ill-formed sequence, as section 3.9 of the Unicode Standard recommends),
  and text comes apart at the same places read backwards. An iterator's
  `base()` is where the code point it is writing begins in the text, and
  `well_formed()` says whether that code point was read or replaced.
  `alef::is_well_formed` says whether all of some text is.
- **Grapheme clusters** (UAX #29, extended). `alef::graphemes` is a view of
  the clusters of text in any UTF, each a `std::ranges::subrange` of the
  text; it is bidirectional if the text is, and needs text it can read more
  than once. `alef::next_grapheme_boundary`, `alef::prev_grapheme_boundary`
  and `alef::is_grapheme_boundary` work on iterators into the text, and the
  properties the rules are decided by are there too:
  `alef::grapheme_cluster_break_of`, `alef::indic_conjunct_break_of` and
  `alef::is_extended_pictographic`. Checked against every line of
  `GraphemeBreakTest.txt` forwards, backwards and at every byte, and the
  same text in UTF-16 and UTF-32 comes apart into the same clusters.

Text is a string, a view of one, a string literal (without the NUL it ends
in), or any other range of code units. A view made of a temporary string
keeps it.

```cpp
import std;
import alef;

static_assert(std::ranges::distance(u8"e\u0301🧑‍💻क्ष" | alef::graphemes) == 3);

void show(std::string_view text) {
  for (auto cluster : text | alef::graphemes | std::views::reverse)
    std::println("{}", std::string_view(cluster));
  for (char32_t code_point : text | alef::as_utf32)
    std::println("U+{:04X}", std::uint32_t(code_point));
  std::u16string wide = text | alef::as_utf16 | std::ranges::to<std::u16string>();
}
```

## Building

CMake 4.3.4 or newer, Ninja, and a compiler that builds C++23 modules with
`import std` and has `#embed`. CI builds with clang 22 and 23 and libc++.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The tests are googletest's, as a module:
[j4niwzis/googletest-modules](https://github.com/j4niwzis/googletest-modules),
fetched by [cmake-everywhere](https://github.com/j4niwzis/cmake-everywhere)
at a pinned commit. Configured with `-DALEF_CONSTEXPR_TESTS=ON`, as CI is,
the compiler runs every test as well, as a constant expression. A test that
is not one does not stop the build: it fails when the tests run, and compiles
itself alone to show what the compiler says of it.

Reading the UCD is more constant evaluation than clang allows by default, so
the `alef` target raises the bound with `-fconstexpr-steps=100000000`, and
raises it for its users as well: with modules, whoever imports alef may
compile its interfaces too. Through CMake there is nothing to do; without
it, pass the flag.

## Unicode data

`ucd/` is the Unicode Character Database, © Unicode, Inc., distributed under
the Unicode License v3 (`ucd/LICENSE.txt`).
