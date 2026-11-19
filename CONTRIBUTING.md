# Contributing to Suzume

Contributions are welcome, including small and unfinished ones. A maintainer reviews every pull request, finishes what is missing, and merges it, so you do not need to get everything in this document right, or even read all of it.

## What a pull request needs

- A description of what the change does and why. If the intent is clear, the maintainer can verify the behaviour. For a tokenization change, the input sentence and the tokens you expect are enough.
- If you can, a build and the tests for the part you changed.

That is all. A red check, a missing test, or a binding you did not touch is fine: say what you know in the pull request, and the maintainer will finish it or tell you what is left. Target `develop` if you can; a pull request against `main` is retargeted.

For a larger change, such as a new analysis stage or a change to the public API, opening an issue first to describe the idea can save you work. It is not required.

Reporting a bug you do not intend to fix is a useful contribution too, and needs none of the setup below. A sentence that Suzume splits in the wrong place is a bug report.

### What the maintainer can verify

The maintainer tests on macOS, on Linux through CI, and on Windows in Japanese only. If your change or bug depends on Windows in another language (the console code page, how Japanese text reaches the CLI, the per-user or system dictionary directory), describe what you see there, ideally with the exact command and its output, because it cannot be reproduced on the maintainer's side.

## What you are contributing to

A Japanese tokenizer with a C++17 core and a C ABI, shipped to several runtimes:

- `src/` and `include/` are the core library and its public C and C++ headers.
- `tools/cli` is the `suzume-cli` command.
- `bindings/wasm` is the `@libraz/suzume` npm package; `bindings/python` is the `suzume` PyPI package.
- `data/` holds the dictionary sources; `tests/` holds the C++ tests and the tokenization test data.
- `scripts/mcp` is the tooling that derives test expectations and edits test data and dictionaries.

The Go binding is maintained in a separate repository, [libraz/go-suzume](https://github.com/libraz/go-suzume). Report problems with the Go module there.

The rest of this document has two parts. [Checklist](#checklist) describes what a finished change looks like. [Reference](#reference) explains how the checks and tools behave.

## Development setup

A C++17 compiler, CMake 3.15 or later, and [uv](https://docs.astral.sh/uv/) 0.10 or later. uv provides the Python interpreter pinned in each project's `.python-version`, and the tooling and Python binding environments from their committed `uv.lock`; no system Python is needed.

```bash
make build                                      # configure and build the core, CLI and tests
make native-test                                # compile the dictionaries and run the C++ tests
SUZUME_DEBUG=1 ./build/bin/suzume-cli "東京に行く"  # analyse one sentence; 1=basic, 2=detailed, 3=trace
make lint                                       # read-only static checks
make format                                     # apply formatting across every language
```

`make lint` also runs Biome on the WebAssembly binding, so it needs the Node and Yarn set up below, and `make format` additionally needs clang-format (`make CLANG_FORMAT=<binary> format` selects a versioned one).

`make python-sync` provisions both Python environments up front; every target that needs one also syncs it on demand. The tooling tests (`make mcp-test`) and `make oracle-sync-check` also need the reference analyzer that CI installs in the `mcp-tests` job.

Python binding (ctypes over the shared library):

```bash
make python-test
```

WebAssembly binding, which needs an Emscripten toolchain plus the Node and Yarn that [mise](https://mise.jdx.dev/) installs from `bindings/wasm/mise.toml` (the Yarn version is read from `packageManager` in `package.json`). The Makefile calls `yarn` directly, so mise must be activated in the shell:

```bash
(cd bindings/wasm && mise install && yarn install)   # no make target installs the JS dependencies
source /path/to/emsdk/emsdk_env.sh
make wasm
make wasm-test
```

`make test` runs every suite above plus the sanitizer, CMake configuration, parity and guardrail checks, so it needs all of these toolchains. Its WASM size check compares against a baseline recorded with the Emscripten release pinned in `ci.yml` and with GNU gzip, so a build from another Emscripten release can fail that check without a real regression.

## What CI requires

A red check on your pull request is not a reason to hold it back; the maintainer can look at it with you.

| Workflow | Job | Runs on | What it checks |
| --- | --- | --- | --- |
| `ci.yml` | `lint` | pushes to `main` and `develop`, and pull requests to `main` and `develop` | Biome on the WASM binding, with Node and Yarn provisioned by mise; ruff on the tooling and repository scripts |
| `ci.yml` | `guardrails` | the same events | the code guardrail ratchet and WASM-core purity, oracle overrides, binding label tables, oracle mirrors of core data, and version mirrors: the package manifests and `uv.lock`, and the uv, mise and Emscripten releases `publish.yml` must share with `ci.yml` |
| `ci.yml` | `mcp-tests` | the same events except pull requests to `develop` | the tooling tests, and that the stored test expectations match a fresh derivation |
| `ci.yml` | `sanitizers` | the same events except pull requests to `develop` | the C++ tests under ASan, LSan and UBSan |
| `ci.yml` | `native-nodebug` | the same events except pull requests to `develop` | the C++ tests with the debug features compiled out |
| `ci.yml` | `score-parity` | the same events except pull requests to `develop`, after `build-and-test` and `native-nodebug` | that the debug build the tests exercise and the shipped build without debug features give every morpheme of the tokenization test inputs the same score |
| `ci.yml` | `cmake-configurations` | the same events except pull requests to `develop` | each supported CMake build and install configuration |
| `ci.yml` | `manylinux-toolchain` | pushes and pull requests to `main` | building and testing the wheel with the oldest supported toolchain |
| `ci.yml` | `build-and-test` | the same events | the C++ tests with coverage, the examples and installed-package consumers, the WASM build, its size check and tests, and native/Python/WASM parity |
| `ci.yml` | `python-binding` | pushes to `main` and pull requests to `main` | ruff, mypy and the tests of the Python binding, and the Python example |
| `publish.yml` | release jobs | version tags | verifies the tag and the version mirrors, runs the C++ tests with and without the debug features, builds and tests the WASM package and the Linux and macOS wheels, publishes to npm and PyPI, and creates the GitHub release |

Changes that touch only Markdown, `docs/` or `LICENSE` do not start `ci.yml`.

## Checklist

This is what a change looks like by the time it is merged. The maintainer checks it and fills in what is missing, so you are not expected to know all of it.

- When a tokenization test fails, change the analyzer or the normalization rules under `scripts/mcp/src/suzume_mcp/core/`, not the expected tokens.
- Fix over-splitting with a bigram connection penalty in `src/analysis/bigram_table.cpp` and under-splitting with a bonus, before reaching for a dictionary entry.
- Solve boundary and scoring problems with conjugation and connection rules (the `grammar::` helpers), bigram costs, or category costs, rather than comparing against specific surface strings.
- Put score values in the `*_constants.h` files instead of writing numeric literals in analysis code.
- Add a dictionary entry only for a closed-class word, or for a proper noun or domain term in the user dictionary under `data/user/`.
- Keep `<fstream>`, `<thread>`, `<filesystem>`, `<mutex>` and `<iostream>` out of the core layers, or inside an `#ifndef __EMSCRIPTEN__` branch.
- Keep morpheme boundaries at particles and inflected forms: `条件に関わらず` stays `条件 / に / 関わら / ず`.
- When a change fixes a tokenization bug, add the sentence as a regression test, through the tooling rather than by editing the JSON.
- Use generic Japanese in tests, examples and comments (`東京`, `りんご`, `テスト`), with no real person's name and no title or character name from a published work.
- Write comments in English.

### Commit messages

Conventional Commits, in English: `type(scope): summary`. A body, when there is one, is a blank line followed by `- ` bullets, one per line. Commit messages can be rewritten at merge, so do not let them hold up a pull request.

## Reference

Nothing in this section asks anything of you; read it when a checklist item or a check puzzles you.

- Test expectations are derived, not hand-written: the output of a reference morphological analyzer passes through the normalization rules in `scripts/mcp/src/suzume_mcp/core/`, and the result is the expectation. Changing one case's expected tokens would silence that case without changing the rule, so a test case carries no expectation of its own, and `suzume_test` and the `guardrails` job both fail on a case that tries to.
- The reference analyzer is a reference, not ground truth. Suzume splits into search units, so it differs on purpose in many places (compounds, numbers and dates, part-of-speech choices), does not reproduce the reference analyzer's errors, and does not aim to match it. The [tokenization differences](https://suzume.libraz.net/docs/mecab-comparison) page lists the intentional ones.
- Scoring generalizes by category; there are no per-word costs. A cost tuned for one word changes how every sentence containing that word is scored, and hides the rule that would have fixed the whole class.
- `scripts/check_code_guardrails.sh` counts, per file under `src/analysis`, `src/grammar`, `src/postprocess` and the `src/core` headers, the comparisons against Japanese string literals, the raw score literals, and the score constants referenced in scorer files, and fails when a count rises above `scripts/guardrail-baseline.tsv`. The same script checks that the file-I/O, thread and stream headers stay out of the core layers outside an `__EMSCRIPTEN__`-excluded branch.
- The WASM build keeps its dictionaries embedded and the core holds only static data, which is why the core cannot use those headers. `scripts/check_wasm_size.sh` fails when the built module, raw or gzipped, grows past a tolerance over its recorded baseline, which large static tables or new hard-coded entries tend to cause.
- A dictionary entry is decided by whether the word belongs to a closed or an open class. Closed classes (particles, auxiliaries, conjunctions, adnominals, pronouns, formal nouns) are finite and live in the built-in dictionary under `src/dictionary/entries/`. Open-class words (general nouns, verbs, adjectives) are derived by rule; the core dictionary under `data/core/` holds only the exceptions rules cannot resolve. The user dictionary under `data/user/` is for proper nouns and domain terms buried in kanji runs.
- Test data in `tests/data/tokenization/*.json` and dictionary sources in `data/**/*.tsv` are written through the MCP server in `scripts/mcp` (wired up in `.mcp.json`): `test_show` shows a sentence's analysis, `test_add` registers a regression test, `dict_add` adds a dictionary entry, and `test_needs_suzume_update` resynchronizes expectations after a normalization rule changes. The agent hooks in `.claude/hooks/` block direct edits of those files. None of this is needed to contribute: describe the sentence and the expected tokens, and the maintainer registers the test.
- A pull request to `develop` runs only `lint`, `guardrails` and `build-and-test`; the rest of `ci.yml` runs on the push to `develop` after the merge, or before the release on `main`.
- The pre-commit hook in `scripts/hooks/pre-commit.sh` is installed by `yarn install` in `bindings/wasm` and checks the formatting of staged files. `SKIP_SIMPLE_GIT_HOOKS=1` skips it.

## Reporting issues

Search the existing issues first, then include the input text, the tokens Suzume produced, the tokens you expected, the Suzume version, which package you used (npm, PyPI, C/C++ library or CLI), and your OS and its display language. For a problem with a user dictionary, include the dictionary file or the entries that trigger it.

Security vulnerabilities go through the process in [SECURITY.md](SECURITY.md), not the public issue tracker.

## Licence

By contributing, you agree that your contributions are licensed under the [Apache License 2.0](LICENSE).
