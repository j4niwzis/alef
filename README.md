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
  the clusters of text in any UTF. Over text that can be read more than
  once, each is a `std::ranges::subrange` of it, and the view is
  bidirectional if the text is. Over text read once, a stream say, each is an
  `alef::grapheme` of its own: its code units, kept in the object up to 32
  bytes' worth and on the heap past that. `text | alef::graphemes(alef::owning<>)`
  asks for those over any text -- text expensive to read more than once, or
  at all, whose every code unit is then read once -- and `alef::owning<N>`
  says how many code units to keep inline, `alef::owning_in_bytes<N>`
  how many bytes. `alef::lazy_graphemes` goes one
  cluster at a time, each a range that reads its code units from the text as
  they are asked for: a cluster of any length in the same few bytes, for text
  nobody vouches for. `alef::next_grapheme_boundary`, `alef::prev_grapheme_boundary`
  and `alef::is_grapheme_boundary` work on iterators into the text, and the
  properties the rules are decided by are there too:
  `alef::grapheme_cluster_break_of`, `alef::indic_conjunct_break_of` and
  `alef::is_extended_pictographic`, all three one byte a code point in a
  table of two stages built from the ranges while the interface is compiled:
  32 KB, and two reads a code point. Checked against every line of
  `GraphemeBreakTest.txt` forwards, backwards and at every byte, and the
  same text in UTF-16 and UTF-32 comes apart into the same clusters.

`import alef;` is all of it, and each part is a module of its own as well --
`alef.utf`, `alef.grapheme`, `alef.word`, `alef.normalization`,
`alef.properties`, `alef.casing`, `alef.line` -- with a CMake target of
its own: `alef::utf`, `alef::grapheme`, `alef::word`, `alef::normalization`,
`alef::properties`, `alef::casing`, `alef::line`, and `alef::alef` for all. Each table is a module of its
own too, importing nothing of the library but the reader of the UCD, so a
change to the library does not compile a table again.
- **Words** (UAX #29, word boundaries). `alef::words` is a view of the
  pieces between word boundaries of text in any UTF -- words, and the spaces
  and punctuation between them -- each a `std::ranges::subrange` of the text,
  and bidirectional if the text is. `alef::next_word_boundary`,
  `alef::prev_word_boundary`, `alef::is_word_boundary` and
  `alef::word_break_of` are there too. Checked against every line of
  `WordBreakTest.txt` forwards, backwards and at every byte, and in UTF-16
  and UTF-32.
- **General properties and width.** `alef::general_category_of`,
  `alef::east_asian_width_of`, `alef::script_of` and
  `alef::script_extensions_of`. A script is `alef::script`, found by its
  name or its ISO 15924 code -- `alef::script("Latin") ==
  alef::script("Latn")` -- since which scripts there are is data that grows
  with Unicode. `alef::width(text)` is the columns text in any UTF takes, a
  terminal say: each grapheme cluster its first code point's width --
  `alef::width_of`, 0, 1 or 2, and for what East_Asian_Width calls
  ambiguous, one or two as asked -- but two where U+FE0F asks for emoji, and
  one where U+FE0E asks for text. Checked against every code point of the
  files they are read from, and every script both ways.
- **Case.** `alef::as_lower`, `alef::as_upper`, `alef::as_title` and
  `alef::as_folded` give text in any UTF in the case asked for, as code
  points, by the full mappings -- `\u00DF` is `SS` in uppercase -- and the
  conditions of SpecialCasing.txt: a final sigma always, and Lithuanian and
  Turkish or Azeri as asked, `text | alef::as_lower(alef::casing_language::turkic)`.
  Titlecase goes word by word. `alef::equal_ignoring_case` compares folded
  text, and `alef::to_lower` and the others map one code point to one.
  Checked against every simple mapping of UnicodeData.txt, every folding of
  CaseFolding.txt and every unconditional mapping of SpecialCasing.txt.
- **Line breaking** (UAX #14). `text | alef::line_breaks` gives the pieces
  of text in any UTF between the places a line may end, each with the spaces
  after it and whether a line has to end there -- after a line feed and the
  like -- by the rules of Unicode 18.0.0, LB1 to LB31. `alef::line_break_of`
  gives a code point's class. Checked against every line of
  `LineBreakTest.txt`, and in UTF-16 and UTF-32.
- **Normalization** (UAX #15). `alef::as_nfc`, `alef::as_nfd`,
  `alef::as_nfkc` and `alef::as_nfkd` read text in any UTF and give it in the
  form as code points -- `| alef::as_utf8` makes it UTF-8 again -- a piece at
  a time: from a code point nothing after it is reordered past or composed
  with anything before, to the next such. `alef::is_nfc` and the others ask
  the quick check first and normalize only where it says maybe, and
  `alef::canonical_combining_class` is there too. Checked against every line
  of `NormalizationTest.txt` in all four forms, and against every code point
  it does not list, which is its own normalization in each.

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
