# alef

Unicode for C++23: text kept as UTF-8, and the Unicode Character Database read
as it is published.

Nothing is generated. `ucd/` holds files of the UCD 18.0.0 exactly as the
Unicode Consortium publishes them, `#embed` puts their bytes into the module,
and constexpr code reads them while the module interface is compiled. A new
version of Unicode is new files in `ucd/`.

## What there is

- **UTF-8.** `alef::decode`, `alef::encode`, `alef::is_well_formed`, and
  `alef::code_points(text)`, a view of the code points of some UTF-8. Bytes
  that are not UTF-8 read as U+FFFD, one for each maximal subpart of an
  ill-formed sequence.
- **Grapheme clusters** (UAX #29, extended): `alef::graphemes(text)`, a view
  whose elements are pieces of the text it was given,
  `alef::next_grapheme_boundary`, and the properties the rules are decided by:
  `grapheme_break`, `conjunct_break`, `extended_pictographic`. Checked against
  every line of `GraphemeBreakTest.txt`.

All of it is constexpr, in the module and wherever it is imported.

```cpp
import std;
import alef;

static_assert(std::ranges::distance(alef::graphemes(u8"e\u0301🧑‍💻क्ष")) == 3);

for (std::string_view cluster : alef::graphemes(text))
  ...
```

## Building

CMake 4.3.4 or newer, Ninja, and a compiler that builds C++23 modules with
`import std` and has `#embed`. CI builds with clang 22 and libc++.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

No flags are needed: the UCD is read in pieces small enough for a compiler's
own bounds on constant evaluation.

## Unicode data

`ucd/` is the Unicode Character Database, © Unicode, Inc., distributed under
the Unicode License v3 (`ucd/LICENSE.txt`).
