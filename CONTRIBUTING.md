# Contributing to Suzume

Thanks for your interest in Suzume! Contributions are welcome.

Suzume is a lightweight Japanese tokenizer with a small dictionary. Before you start, one thing is worth knowing up front: unlike many projects, Suzume enforces a few **design invariants in CI** (see below). They are what keep the analyzer generalizable instead of a pile of special cases, so a change that violates them will fail CI even if the tests pass. Skimming that section first will save you a red pipeline.

## How to Contribute

1. **Fork the repository**
2. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make your changes**
4. **Test your changes**
   ```bash
   make test
   ```
5. **Commit and push**, then **open a Pull Request** against the `develop` branch.

## Development Setup

Requirements: a C++17 compiler and CMake 3.15+.

```bash
make build   # Configure + build (or: cmake -B build && cmake --build build --parallel)
make test    # Run the test suite
make format  # Format code (clang-format), optional
```

Debug a single sentence without running the whole suite:

```bash
SUZUME_DEBUG=1 ./build/bin/suzume-cli "問題文"   # 1=basic, 2=detailed, 3=trace
```

WebAssembly build (for the `@libraz/suzume` npm binding):

```bash
source /path/to/emsdk/emsdk_env.sh
make wasm
```

Python binding (ctypes over the shared library):

```bash
make python-test
```

## Design Invariants (enforced by CI)

These are checked by the `guardrails` and `build-and-test` jobs and are **blocking**. They exist because Suzume aims to be correct *by rule*, not by enumerating cases.

### 1. Fix the implementation, not the expectations

Test expectations are **derived**, not hand-written. The pipeline is:

```
reference analyzer output → normalization rules → test expectation
```

When a test fails, the fix is almost always in the analyzer or the normalization rules — **not** in the expected tokens. Do not edit a test's expected output to match Suzume's current (wrong) output. Test data under `tests/data/tokenization/*.json` and dictionary data under `data/**/*.tsv` are managed through tooling, not edited by hand (a hook blocks direct writes).

Note: the reference analyzer (MeCab) is a *reference*, not ground truth. Suzume is a tokenizer aimed at search-friendly units, so it intentionally differs in many places (compound merging, number/date merging, POS choices). "Matching MeCab" is not the goal, and MeCab bugs are not reproduced — see the [tokenization differences](https://suzume.libraz.net/docs/mecab-comparison) page.

### 2. Generalize with rules — no hardcoding

The `guardrails` job ratchets down the number of surface-string comparisons (`surface == "..."`) and raw score literals in `src/analysis/*.cpp`. Adding either will fail CI. Solve boundary and scoring problems with:

- Conjugation / connection **rules** (`grammar::` helpers)
- **bigram** connection penalties/bonuses (`src/analysis/bigram_table.cpp`) for over- or under-splitting
- Category costs, with constants defined in the `*_constants.h` files

There are **no per-word costs** — scoring generalizes by category, never by individual word.

### 3. Keep the core WASM-pure

The core must build to WebAssembly. Do not include `<fstream>`, `<thread>`, `<filesystem>`, or `<mutex>` unguarded in the core layers — wrap them in `#ifndef __EMSCRIPTEN__` or avoid them. There is also a WASM size-regression check; large additions of static data or hardcoded entries will trip it.

### 4. Dictionaries are the last resort

The decision rule for adding a dictionary entry is **"closed class or open class?"**:

- **Closed class** (grammatically finite — particles, auxiliaries, conjunctions): may live in the L1/L2 dictionaries.
- **Open class** (general nouns, verbs, adjectives — derivable by rule): do **not** add these; handle them with rules.
- **Over-splitting** is fixed with a bigram penalty, **not** by adding a dictionary entry.

Proper nouns and domain terms buried in kanji runs (names, places) are the main legitimate use of the User dictionary.

## Pull Request Guidelines

- **Target `develop`**, not `main`.
- **Tests pass** (`make test`) and the `guardrails` job is green.
- **One change per PR** — keep it focused and atomic.
- **Comments in English**; follow the surrounding style.
- If you fixed a tokenization bug, **add the sentence as a regression test** (via the tooling — see below).

## Using Claude Code (optional)

Day-to-day development in this repo is driven through an MCP server under `scripts/mcp/` (the `.mcp.json` at the root wires it up). If you use Claude Code, tools like `test_show`, `test_add`, and `dict_add` automate the test-expectation pipeline and dictionary edits described above, so you don't touch the JSON/TSV files directly. This is entirely optional — everything above works from a normal shell with `make`.

## Reporting Issues

- Check whether the issue already exists.
- Include the input sentence, the tokens Suzume produced, and what you expected.

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).
