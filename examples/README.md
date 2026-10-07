# Suzume examples

These programs are compiled or type-checked by the repository build.

| File | API | Purpose |
|------|-----|---------|
| [`hello.c`](hello.c) | C ABI | Minimal native consumer |
| [`hello.cpp`](hello.cpp) | C++ wrapper | Minimal native consumer |
| [`cpp/basic.cpp`](cpp/basic.cpp) | C++ public wrapper | Tokenization with POS and lemmas |
| [`cpp/search_indexer.cpp`](cpp/search_indexer.cpp) | C++ public wrapper | Inverted search index |
| [`cpp/tags.cpp`](cpp/tags.cpp) | C++ public wrapper | Search-tag generation |
| [`cpp/user_dictionary.cpp`](cpp/user_dictionary.cpp) | C++ public wrapper | Runtime user dictionary |
| [`ts/basic.ts`](ts/basic.ts) | WASM/TypeScript | Tokenization with POS and lemmas |
| [`ts/search_indexer.ts`](ts/search_indexer.ts) | WASM/TypeScript | Inverted search index |
| [`ts/tags.ts`](ts/tags.ts) | WASM/TypeScript | Search-tag generation |
| [`ts/user_dictionary.ts`](ts/user_dictionary.ts) | WASM/TypeScript | Runtime user dictionary |
| [`python/basic.py`](python/basic.py) | Python package | Tokenization with POS, lemmas and normalized offsets |

## Build and check in-tree

```bash
make examples   # all C/C++ examples
make wasm-test  # builds/tests the package and executes all TypeScript examples
make python-test # tests the package and executes the Python example
./build/bin/suzume_example_cpp "東京に行きました"
```

The C++ examples link the single in-tree `suzume` target. A downstream project
should use `find_package(suzume CONFIG REQUIRED)` as shown below.

## Build against an installed package (find_package)

[`consumer/`](consumer) is a standalone project that finds Suzume with
`find_package(suzume CONFIG REQUIRED)`. It doubles as the packaging smoke test:

```bash
make install PREFIX=/tmp/suzume            # install the library + package config
cmake -S examples/consumer -B build-consumer -DCMAKE_PREFIX_PATH=/tmp/suzume
cmake --build build-consumer
ctest --test-dir build-consumer --output-on-failure
```

`make consumer-smoke` runs the whole install → find_package → run sequence in one step.

## ABI compatibility

`find_package` and pkg-config pin the header and the library together, so they
always agree. A consumer that resolves the library separately — a language
binding that `dlopen`s `libsuzume`, or a build that links a system copy — can
end up with a header and a library from different revisions. Removing an
exported symbol fails loudly there, but a public struct that changed size or
field order does not: the calls succeed and read the wrong bytes.

`suzume_c.h` defines `SUZUME_ABI_VERSION` for exactly that case. Compare it
against what the loaded library reports before calling anything else:

```c
if (suzume_abi_version() != SUZUME_ABI_VERSION) {
  /* Header and library disagree; the struct layouts are not trustworthy. */
  return -1;
}
```

The number changes when a public struct changes size or field order, or when an
exported entry point is removed or changes signature. Adding an entry point that
leaves the existing ones untouched does not change it.

## Embedded (no filesystem)

Bake the dictionaries into the binary so the library needs no data files at
runtime:

```bash
make embedded          # -DSUZUME_EMBED_DICT=ON, static, no CLI/tests
```
