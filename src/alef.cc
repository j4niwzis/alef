// alef: Unicode for C++.
export module alef;

export import :utf;
export import :grapheme;
// Nothing of it is exported by name. It is here because an interface
// partition has to be, and because what the other partitions read from it
// has to be reachable wherever they are evaluated.
export import :ucd;
