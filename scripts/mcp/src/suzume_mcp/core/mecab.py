"""MeCab interface - subprocess-based MeCab analysis."""

import asyncio
import shlex


def mecab_analyze(text: str) -> list[dict]:
    """Run MeCab on text and return parsed tokens (synchronous)."""
    import subprocess

    escaped = shlex.quote(text)
    result = subprocess.run(
        f"echo {escaped} | mecab",
        shell=True,
        capture_output=True,
        text=True,
    )
    return _parse_mecab_output(result.stdout)


async def mecab_analyze_async(text: str) -> list[dict]:
    """Run MeCab on text and return parsed tokens (async)."""
    proc = await asyncio.create_subprocess_exec(
        "mecab",
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, _ = await proc.communicate(input=(text + "\n").encode("utf-8"))
    return _parse_mecab_output(stdout.decode("utf-8"))


def _parse_mecab_output(output: str) -> list[dict]:
    """Parse MeCab tab-separated output into token dicts."""
    tokens = []
    for line in output.split("\n"):
        if line == "EOS" or line == "":
            break
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
