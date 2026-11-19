# suzume

[![PyPI](https://img.shields.io/pypi/v/suzume)](https://pypi.org/project/suzume/)
[![npm](https://img.shields.io/npm/v/@libraz/suzume)](https://www.npmjs.com/package/@libraz/suzume)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](https://github.com/libraz/suzume/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/docs-suzume.libraz.net-2563eb)](https://suzume.libraz.net)

**Japanese tokenization for Python applications.** Suzume is not a full
morphological analyzer like MeCab: it prioritizes useful search units while
still providing part-of-speech tags, base forms, and keyword extraction. The wheel
requires no extra dictionary installation.

📖 **[Full API reference and guides](https://suzume.libraz.net/docs/python)**

See the [MeCab comparison](https://suzume.libraz.net/docs/mecab-comparison) for
concrete examples of token boundaries, base forms, and trade-offs.

## Installation

```bash
pip install suzume
```

PyPI publishes binary wheels for Linux x86_64
(`manylinux2014`/`manylinux_2_17`) and macOS arm64 (macOS 11 or newer).
Windows, macOS x86_64, Linux arm64, and other platforms or architectures are
not supported. Suzume does not publish or support a source distribution; an
installation succeeds only when a compatible wheel is available.

## Quick Start

```python
from suzume import Suzume

with Suzume() as sz:
    tokens = sz.analyze("すもももももももものうち")
    tags = sz.generate_tags("東京の公園に行きました")
```

The wheel loads its bundled dictionaries from the package directory, so
`Suzume()` takes no data-directory argument and does not consult
`SUZUME_DATA_DIR`.

## Filtering generated tags by part of speech

`pos_filter` takes the labels that `Tag.pos` prints: `NOUN`, `VERB`, `ADJ`,
`ADV`, `PARTICLE`, and `AUX`, as one name or a list, tuple or set of names:

```python
tags = sz.generate_tags("東京の公園に行きました", pos_filter=["NOUN", "VERB"])
```

`NOUN` also keeps pronouns (`PRON`). Naming `PARTICLE` or `AUX` returns those
tags even though they are excluded by default. Any other value raises
`ValueError`. Leaving `pos_filter` out, `None`, or an empty container keeps
every part of speech.

## Error handling

An invalid argument or option raises `ValueError`. A failure inside the
library, such as a rejected user dictionary, raises `SuzumeError`, carrying the
library's message, an `ErrorCode` and the name of the operation. Using an
instance after `close()` raises `SuzumeError` with `ErrorCode.INVALID_INPUT`.

```python
from suzume import ErrorCode, Suzume, SuzumeError

try:
    installed = sz.load_user_dictionary(tsv)
except SuzumeError as error:
    if error.code == ErrorCode.PARSE:
        print(error)  # the library's message
```

## Threads

Calls on one `Suzume` instance are serialized, so it can safely be shared
between Python threads. Create one instance per worker when native analysis
should run in parallel.

Installing the wheel also provides the `suzume` command:

```bash
suzume "東京へ行く"
suzume analyze --mode search --format json "東京の公園"
printf 'りんごを食べる\n' | suzume --format tags
```

Run `suzume --help` for analysis modes, normalization controls, output formats,
tag filters, and repeatable `-d`/`--dictionary` options. Dictionary compilation,
validation, and test commands remain part of the separate native developer
tool, `suzume-cli`.

See the [Python API](https://suzume.libraz.net/docs/python) for the complete
interface, including user dictionaries and tag-generation options.

## Also available

```bash
npm install @libraz/suzume  # JavaScript / TypeScript WebAssembly package
```

The native C and C++ library is documented at
[suzume.libraz.net/docs/cpp](https://suzume.libraz.net/docs/cpp).

## License

[Apache License 2.0](https://github.com/libraz/suzume/blob/main/LICENSE)
