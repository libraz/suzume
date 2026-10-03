"""MeCab interface - subprocess-based MeCab analysis."""

import asyncio
import functools


def mecab_analyze(text: str) -> list[dict]:
    """Run MeCab on text and return parsed tokens (synchronous)."""
    import subprocess

    result = subprocess.run(
        ["mecab"],
        input=text + "\n",
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"MeCab failed: {result.stderr.strip() or 'non-zero exit'}")
    if "EOS" not in result.stdout.splitlines():
        raise RuntimeError("MeCab failed: missing EOS marker")
    return _parse_mecab_output(result.stdout)


async def mecab_analyze_async(text: str) -> list[dict]:
    """Run MeCab on text and return parsed tokens (async)."""
    proc = await asyncio.create_subprocess_exec(
        "mecab",
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, stderr = await proc.communicate(input=(text + "\n").encode("utf-8"))
    if proc.returncode != 0:
        raise RuntimeError(f"MeCab failed: {stderr.decode('utf-8').strip() or 'non-zero exit'}")
    output = stdout.decode("utf-8")
    if "EOS" not in output.splitlines():
        raise RuntimeError("MeCab failed: missing EOS marker")
    return _parse_mecab_output(output)


def is_single_token_of_pos(surface: str, pos: str) -> bool:
    """Whether the reference dictionary reads a surface as exactly one token of a POS.

    A rule that reconstructs a base form has to know whether the form it built
    is a word. Asking the dictionary for the whole surface answers that: a
    headword comes back as one token of the expected class, while anything the
    analyzer has to assemble comes back split or under another class.
    """
    tokens = mecab_analyze(surface)
    return len(tokens) == 1 and tokens[0].get("pos") == pos and tokens[0].get("surface") == surface


@functools.lru_cache(maxsize=4096)
def is_na_adjective_stem(surface: str) -> bool:
    """Whether the reference dictionary reads a surface as a na-adjective stem.

    A stem comes back as a plain noun before だ or の and as a na-adjective stem
    only where the attributive な forces it (ふつう, 非常), so the attributive
    probe recovers the class whatever the surrounding context or script.
    """
    probe = mecab_analyze(surface + "な")
    return bool(probe) and probe[0].get("surface") == surface and probe[0].get("pos_sub1") == "形容動詞語幹"


def _parse_mecab_output(output: str) -> list[dict]:
    """Parse MeCab tab-separated output into token dicts.

    Multi-line input produces several EOS-delimited blocks; all of them are
    parsed and their tokens concatenated in order. EOS and blank separator
    lines are skipped rather than treated as a terminator, so nothing after
    an embedded newline is dropped.
    """
    tokens = []
    for line in output.split("\n"):
        if line == "EOS" or line == "":
            continue
        parts = line.split("\t", 1)
        if len(parts) < 1:
            continue
        surface = parts[0]
        if not surface:
            continue

        features = parts[1].split(",") if len(parts) > 1 else []
        tokens.append(
            {
                "surface": surface,
                "pos": features[0] if len(features) > 0 else "",
                "pos_sub1": features[1] if len(features) > 1 else "",
                "pos_sub2": features[2] if len(features) > 2 else "",
                "pos_sub3": features[3] if len(features) > 3 else "",
                "conj_type": features[4] if len(features) > 4 else "",
                "conj_form": features[5] if len(features) > 5 else "",
                "lemma": features[6] if len(features) > 6 else surface,
                "reading": features[7] if len(features) > 7 else "",
            }
        )
    return tokens
