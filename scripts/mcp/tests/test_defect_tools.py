"""Tests for the defect store MCP tools."""

import pytest

from suzume_mcp.core import bug_store
from suzume_mcp.tools.defect_tools import (
    _parse_ids,
    defect_add,
    defect_archive,
    defect_dismiss,
    defect_get,
    defect_list,
    defect_probe,
    defect_recheck,
    defect_reopen,
    defect_report,
    defect_resolve,
    defect_update,
    defect_yield,
)
from tests.helpers import parse as parse_json
from tests.helpers import run


@pytest.fixture
def store(tmp_path, monkeypatch):
    """Point every source at a temporary directory; returns the "defect" one."""
    path = tmp_path / "bugs"
    for source in bug_store.KNOWN_SOURCES:
        monkeypatch.setitem(bug_store._SOURCE_DIRS, source, path if source == "defect" else tmp_path / source)
    return path


@pytest.fixture
def engine(monkeypatch):
    """Replace the oracle and the CLI with in-memory tables.

    Both default to "the record still reproduces" so a test only declares the
    texts whose behavior it cares about.
    """

    tables = {"oracle": {}, "cli": {}, "skip_user_dict": []}

    def fake_oracle(texts):
        return [([{"surface": tok} for tok in tables["oracle"].get(text, text).split()], "test", "") for text in texts]

    async def fake_cli(text, cli_path=None, skip_user_dict=False):
        tables["skip_user_dict"].append(skip_user_dict)
        return tables["cli"].get(text, text).split()

    monkeypatch.setattr("suzume_mcp.tools.defect_tools.get_expected_tokens_batch_subprocess", fake_oracle)
    monkeypatch.setattr("suzume_mcp.tools.defect_tools.get_suzume_surfaces_async", fake_cli)
    return tables


def file_record(**kwargs):
    params = {"text": "テスト文", "expected": "テスト 文", "suzume": "テスト文"}
    params.update(kwargs)
    return bug_store.create("defect", **params)


# ============================================================================
# Id parsing
# ============================================================================


class TestParseIds:
    @pytest.mark.parametrize("raw", ["12,52", "12 52", "#12, #52", " 12 , 52 "])
    def test_accepted_forms(self, raw):
        assert _parse_ids(raw) == [12, 52]

    @pytest.mark.parametrize("raw", ["", "   ", "all", "ALL", "*", "any"])
    def test_bulk_shorthands_are_rejected(self, raw):
        with pytest.raises(ValueError):
            _parse_ids(raw)

    def test_a_non_numeric_entry_is_rejected(self):
        with pytest.raises(ValueError):
            _parse_ids("12,foo")


# ============================================================================
# defect_add
# ============================================================================


class TestDefectAdd:
    def test_registers_a_record(self, store, engine):
        result = parse_json(
            run(defect_add(text="女らしい服装", expected="女らしい 服装", suzume="女 らしい 服装", pattern="derived"))
        )
        assert result["status"] == "ok"
        assert result["id"] == 1
        assert result["diff_type"] == "over-split"
        assert result["check"] == "surface"
        assert result["open_total"] == 1

    def test_missing_sides_are_derived_from_the_oracle_and_the_cli(self, store, engine):
        engine["oracle"]["女らしい服装"] = "女らしい 服装"
        engine["cli"]["女らしい服装"] = "女 らしい 服装"

        run(defect_add(text="女らしい服装"))

        record = bug_store.load_open("defect")[0]
        assert record["expected"] == "女らしい 服装"
        assert record["suzume"] == "女 らしい 服装"

    def test_a_second_report_of_the_same_text_is_refused(self, store, engine):
        run(defect_add(text="テスト文", expected="テスト 文", suzume="テスト文"))
        result = parse_json(run(defect_add(text="テスト文", expected="テスト 文", suzume="テスト文")))

        assert result["status"] == "duplicate"
        assert result["existing"]["id"] == 1
        assert len(bug_store.load_open("defect")) == 1

    def test_force_overrides_the_duplicate_check(self, store, engine):
        run(defect_add(text="テスト文", expected="テスト 文", suzume="テスト文"))
        result = parse_json(run(defect_add(text="テスト文", expected="テスト 文", suzume="テスト文", force=True)))

        assert result["status"] == "ok"
        assert len(bug_store.load_open("defect")) == 2

    def test_slash_separated_input_is_stored_canonically(self, store, engine):
        run(defect_add(text="女らしい服装", expected="女らしい / 服装", suzume="女 / らしい / 服装"))
        assert bug_store.load_open("defect")[0]["expected"] == "女らしい 服装"

    def test_a_pos_only_report_is_marked_manual(self, store, engine):
        result = parse_json(run(defect_add(text="本の多く", expected="本 の 多く", suzume="本 の 多く")))
        assert result["check"] == "manual"

    def test_empty_text_is_rejected(self, store, engine):
        assert parse_json(run(defect_add(text="  ")))["status"] == "error"

    def test_an_unknown_priority_is_rejected(self, store, engine):
        result = parse_json(run(defect_add(text="a", expected="a b", suzume="ab", priority="urgent")))
        assert result["status"] == "error"


# ============================================================================
# defect_list / defect_get / defect_update
# ============================================================================


class TestDefectList:
    def _fill(self, count=5):
        for idx in range(1, count + 1):
            file_record(text=f"文{idx}", expected="a b", suzume="ab", pattern=f"p{idx % 2}")

    def test_rows_are_taken_from_the_start_of_the_range(self, store, engine):
        self._fill()
        result = parse_json(run(defect_list(limit=2)))
        assert [rec["id"] for rec in result["records"]] == [1, 2]

    def test_offset_pages_forward(self, store, engine):
        self._fill()
        result = parse_json(run(defect_list(limit=2, offset=2)))
        assert [rec["id"] for rec in result["records"]] == [3, 4]

    def test_counts_cover_the_whole_store_while_rows_are_filtered(self, store, engine):
        self._fill()
        result = parse_json(run(defect_list(pattern="p1", limit=1)))
        assert result["summary"]["total"] == 5
        assert result["matched"] == 3
        assert result["returned"] == 1

    def test_manual_records_can_be_singled_out(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        file_record(text="b", expected="c d", suzume="c d")
        result = parse_json(run(defect_list(check="manual")))
        assert result["matched"] == 1


class TestDefectGet:
    def test_returns_full_records_with_their_current_state(self, store, engine):
        file_record(text="テスト文", expected="テスト 文", suzume="テスト文")
        engine["oracle"]["テスト文"] = "テスト 文"
        engine["cli"]["テスト文"] = "テスト文"

        result = parse_json(run(defect_get(ids="1")))

        assert result["records"][0]["state"] == "open"
        assert result["records"][0]["current"] == "テスト文"

    def test_unknown_ids_are_reported(self, store, engine):
        result = parse_json(run(defect_get(ids="7", live=False)))
        assert result["missing"] == [7]


class TestDefectUpdate:
    def test_fields_are_amended(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        result = parse_json(run(defect_update(id=1, pattern="compound-verb", priority="high")))
        assert result["status"] == "ok"
        assert bug_store.load_open("defect")[0]["pattern"] == "compound-verb"

    def test_an_unknown_record_is_rejected(self, store, engine):
        assert parse_json(run(defect_update(id=9, pattern="x")))["status"] == "error"

    def test_a_call_with_no_changes_is_rejected(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        assert parse_json(run(defect_update(id=1)))["status"] == "error"

    def test_kind_can_be_corrected_after_triage(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab", kind="tokenizer")

        result = parse_json(run(defect_update(id=1, kind="oracle")))

        assert result["record"]["kind"] == "oracle"
        assert bug_store.load_open("defect")[0]["kind"] == "oracle"

    def test_an_unknown_kind_is_rejected(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        assert parse_json(run(defect_update(id=1, kind="unknown")))["status"] == "error"

    def test_expected_change_recomputes_derived_fields_unless_forced(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")

        result = parse_json(run(defect_update(id=1, expected="ab")))

        assert result["record"]["diff_type"] == "match"
        assert result["record"]["check"] == "manual"
        assert (store / "0001_match.json").is_file()


# ============================================================================
# defect_recheck
# ============================================================================


class TestDefectRecheck:
    def test_states_are_classified(self, store, engine):
        file_record(text="直った", expected="直 った", suzume="直った")
        file_record(text="まだ", expected="ま だ", suzume="まだ")
        file_record(text="動いた", expected="動 い た", suzume="動いた")
        file_record(text="品詞のみ", expected="品詞 のみ", suzume="品詞 のみ")

        engine["oracle"].update({"直った": "直 った", "まだ": "ま だ", "動いた": "動 い た", "品詞のみ": "品詞 のみ"})
        engine["cli"].update(
            {
                "直った": "直 った",  # matches the oracle now
                "まだ": "まだ",  # unchanged since it was filed
                "動いた": "動い た",  # still wrong, but the output moved
                "品詞のみ": "品詞 のみ",
            }
        )

        result = parse_json(run(defect_recheck()))

        assert result["counts"] == {"needs-manual": 1, "open": 1, "resolved": 1, "stale": 1}
        assert result["applied"] is False
        assert result["open_total"] == 4

    def test_a_read_only_run_never_moves_a_record(self, store, engine):
        file_record(text="直った", expected="直 った", suzume="直った")
        engine["oracle"]["直った"] = "直 った"
        engine["cli"]["直った"] = "直 った"

        run(defect_recheck())

        assert len(bug_store.load_open("defect")) == 1
        assert bug_store.load_resolved("defect") == []

    def test_apply_retires_only_the_resolved_records(self, store, engine):
        file_record(text="直った", expected="直 った", suzume="直った")
        file_record(text="まだ", expected="ま だ", suzume="まだ")
        engine["oracle"].update({"直った": "直 った", "まだ": "ま だ"})
        engine["cli"].update({"直った": "直 った", "まだ": "まだ"})

        result = parse_json(run(defect_recheck(apply=True)))

        assert result["retired"] == [1]
        assert [rec["id"] for rec in bug_store.load_open("defect")] == [2]
        assert [rec["id"] for rec in bug_store.load_resolved("defect")] == [1]

    def test_apply_never_closes_a_pos_only_record(self, store, engine):
        file_record(text="品詞のみ", expected="品詞 のみ", suzume="品詞 のみ")
        engine["oracle"]["品詞のみ"] = "品詞 のみ"
        engine["cli"]["品詞のみ"] = "品詞 のみ"

        result = parse_json(run(defect_recheck(apply=True)))

        assert result["retired"] == []
        assert len(bug_store.load_open("defect")) == 1

    def test_a_subset_can_be_rechecked(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        file_record(text="b", expected="c d", suzume="cd")
        result = parse_json(run(defect_recheck(ids="2")))
        assert result["checked"] == 1

    def test_unknown_and_resolved_ids_are_reported_as_missing(self, store, engine):
        file_record(text="open", expected="o pen", suzume="open")
        file_record(text="closed", expected="cl osed", suzume="closed")
        closed = bug_store.load_one("defect", 2)
        bug_store.resolve("defect", closed, note="fixed", output="cl osed")

        result = parse_json(run(defect_recheck(ids="1,2,9")))

        assert result["checked"] == 1
        assert result["missing"] == [2, 9]

    def test_a_bulk_shorthand_is_rejected(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        assert parse_json(run(defect_recheck(ids="all")))["status"] == "error"


# ============================================================================
# defect_resolve / defect_reopen
# ============================================================================


class TestDefectResolve:
    def test_a_fixed_record_is_moved_to_resolved(self, store, engine):
        file_record(text="直った", expected="直 った", suzume="直った")
        engine["oracle"]["直った"] = "直 った"
        engine["cli"]["直った"] = "直 った"

        result = parse_json(run(defect_resolve(ids="1", note="plan p1 package 2")))

        assert result["status"] == "ok"
        assert bug_store.load_open("defect") == []
        assert bug_store.load_resolved("defect")[0]["resolved_note"] == "plan p1 package 2"

    def test_a_record_that_still_reproduces_is_refused(self, store, engine):
        file_record(text="まだ", expected="ま だ", suzume="まだ")
        engine["oracle"]["まだ"] = "ま だ"
        engine["cli"]["まだ"] = "まだ"

        result = parse_json(run(defect_resolve(ids="1")))

        assert result["refused"][0]["reason"] == "still reproduces"
        assert len(bug_store.load_open("defect")) == 1

    def test_a_pos_only_record_is_refused_with_its_own_reason(self, store, engine):
        file_record(text="品詞のみ", expected="品詞 のみ", suzume="品詞 のみ")
        result = parse_json(run(defect_resolve(ids="1")))
        assert "POS/lemma-only" in result["refused"][0]["reason"]

    def test_force_closes_a_failing_record_but_demands_a_note(self, store, engine):
        file_record(text="まだ", expected="ま だ", suzume="まだ")

        assert parse_json(run(defect_resolve(ids="1", force=True)))["status"] == "error"
        assert len(bug_store.load_open("defect")) == 1

        result = parse_json(run(defect_resolve(ids="1", force=True, note="verified by hand")))
        assert result["status"] == "ok"
        assert bug_store.load_resolved("defect")[0]["resolved_note"] == "verified by hand"

    def test_ids_are_required(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        assert parse_json(run(defect_resolve(ids="")))["status"] == "error"
        assert parse_json(run(defect_resolve(ids="all")))["status"] == "error"
        assert len(bug_store.load_open("defect")) == 1


class TestDefectReopen:
    def test_a_retired_record_comes_back(self, store, engine):
        record = file_record(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")

        result = parse_json(run(defect_reopen(ids="1")))

        assert result["open_total"] == 1
        assert bug_store.load_resolved("defect") == []

    def test_an_unknown_record_is_reported(self, store, engine):
        assert parse_json(run(defect_reopen(ids="5")))["missing"] == [5]


# ============================================================================
# defect_report
# ============================================================================


class TestDefectReport:
    def test_the_same_store_renders_identically(self, store, engine, tmp_path):
        file_record(text="a", expected="a b", suzume="ab", pattern="p1")
        file_record(text="b", expected="c d", suzume="cd", pattern="p0")

        first = parse_json(run(defect_report()))["report"]
        second = parse_json(run(defect_report()))["report"]

        assert first == second

    def test_it_writes_to_the_requested_path(self, store, engine, tmp_path):
        file_record(text="a", expected="a b", suzume="ab")
        target = tmp_path / "backup" / "reports" / "defect.md"
        monkeypatch = pytest.MonkeyPatch()
        monkeypatch.setattr("suzume_mcp.tools.defect_tools.PROJECT_ROOT", tmp_path)
        try:
            result = parse_json(run(defect_report(out="backup/reports/defect.md")))
        finally:
            monkeypatch.undo()

        assert result["written"] == str(target)
        assert "Defect store report" in target.read_text(encoding="utf-8")

    @pytest.mark.parametrize("out", ["../outside.md", "data/core/unsafe.md", "/tmp/unsafe.md"])
    def test_it_refuses_report_paths_outside_backup(self, store, engine, out):
        file_record(text="a", expected="a b", suzume="ab")

        result = parse_json(run(defect_report(out=out)))

        assert result["status"] == "error"

    def test_a_corrupt_record_is_reported_instead_of_being_hidden(self, store, engine):
        (store / "0001_broken.json").parent.mkdir(parents=True, exist_ok=True)
        (store / "0001_broken.json").write_text("{broken", encoding="utf-8")

        result = parse_json(run(defect_report()))

        assert result["status"] == "error"
        assert "0001_broken.json" in result["message"]

    def test_resolved_history_is_opt_in(self, store, engine):
        record = file_record(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        file_record(text="b", expected="c d", suzume="cd")

        assert "Resolved history" not in parse_json(run(defect_report()))["report"]
        assert "Resolved history" in parse_json(run(defect_report(include_resolved=True)))["report"]


# ============================================================================
# defect_archive
# ============================================================================


class TestDefectArchive:
    def test_the_confirmation_must_match_the_open_count(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        file_record(text="b", expected="c d", suzume="cd")

        for wrong in ("", "defect", "defect:1", "thread:2", "2"):
            assert parse_json(run(defect_archive(confirm=wrong)))["status"] == "error"
        assert len(bug_store.load_open("defect")) == 2

    def test_a_matching_confirmation_moves_the_records(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")

        result = parse_json(run(defect_archive(confirm="defect:1")))

        assert result["archived"] == 1
        assert bug_store.load_open("defect") == []
        archived = list((store / bug_store.ARCHIVE_DIRNAME).rglob("*.json"))
        assert len(archived) == 1

    def test_retired_records_survive_an_archive(self, store, engine):
        record = file_record(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        file_record(text="b", expected="c d", suzume="cd")

        run(defect_archive(confirm="defect:1"))

        assert len(bug_store.load_resolved("defect")) == 1


# ============================================================================
# Analyzer selection
# ============================================================================


_JUDGED_WITHOUT_USER_DICT = {"defect": True, "literary": True, "thread": False}


class TestAnalyzerMatchesTheStore:
    """Only thread records are judged with user.dic; the rest run like the tokenization tests."""

    @pytest.mark.parametrize("source", sorted(_JUDGED_WITHOUT_USER_DICT))
    def test_probe(self, store, engine, source):
        run(defect_probe(texts=["テスト文"], source=source))
        assert engine["skip_user_dict"] == [_JUDGED_WITHOUT_USER_DICT[source]]

    @pytest.mark.parametrize("source", sorted(_JUDGED_WITHOUT_USER_DICT))
    def test_add_deriving_the_observed_output(self, store, engine, source):
        run(defect_add(text="テスト文", expected="テスト 文", source=source))
        assert engine["skip_user_dict"] == [_JUDGED_WITHOUT_USER_DICT[source]]

    @pytest.mark.parametrize("source", sorted(_JUDGED_WITHOUT_USER_DICT))
    def test_recheck(self, store, engine, source):
        bug_store.create(source, text="テスト文", expected="テスト 文", suzume="テスト文")
        run(defect_recheck(source=source))
        assert engine["skip_user_dict"] == [_JUDGED_WITHOUT_USER_DICT[source]]


# ============================================================================
# defect_probe
# ============================================================================


class TestDefectProbe:
    def test_only_the_differing_sentences_come_back(self, store, engine):
        engine["oracle"].update({"同じ": "同じ", "違う": "違 う"})
        engine["cli"].update({"同じ": "同じ", "違う": "違う"})

        result = parse_json(run(defect_probe(texts=["同じ", "違う"])))

        assert result["counts"] == {"match": 1, "candidate": 1}
        assert [row["text"] for row in result["rows"]] == ["違う"]
        assert result["to_judge"] == 1

    def test_matches_can_be_asked_for(self, store, engine):
        engine["oracle"]["同じ"] = "同じ"
        engine["cli"]["同じ"] = "同じ"
        result = parse_json(run(defect_probe(texts=["同じ"], include_match=True)))
        assert [row["state"] for row in result["rows"]] == ["match"]

    def test_a_candidate_carries_both_outputs_and_the_token_delta(self, store, engine):
        engine["oracle"]["違う"] = "違 う"
        engine["cli"]["違う"] = "違う"

        row = parse_json(run(defect_probe(texts=["違う"])))["rows"][0]

        assert row["expected"] == "違 う"
        assert row["suzume"] == "違う"
        assert row["token_delta"] == -1

    def test_an_open_record_comes_back_as_known(self, store, engine):
        file_record(text="既出", expected="既 出", suzume="既出")
        engine["oracle"]["既出"] = "既 出"
        engine["cli"]["既出"] = "既出"

        row = parse_json(run(defect_probe(texts=["既出"])))["rows"][0]

        assert row["state"] == "known"
        assert row["id"] == 1

    def test_a_dismissed_text_never_returns_as_a_candidate(self, store, engine):
        engine["oracle"]["判定済み"] = "判定 済み"
        engine["cli"]["判定済み"] = "判定済み"
        run(
            defect_dismiss(
                text="判定済み",
                resolution="conformant",
                reason="search-unit choice",
                pattern="compound-noun",
            )
        )

        row = parse_json(run(defect_probe(texts=["判定済み"])))["rows"][0]

        assert row["state"] == "dismissed"
        assert row["resolution"] == "conformant"
        assert row["reason"] == "search-unit choice"

    def test_a_fixed_record_that_differs_again_is_a_regression(self, store, engine):
        record = file_record(text="直したのに", expected="直し た のに", suzume="直したのに")
        bug_store.resolve("defect", record, note="fixed", output="直し た のに")
        engine["oracle"]["直したのに"] = "直し た のに"
        engine["cli"]["直したのに"] = "直したのに"

        result = parse_json(run(defect_probe(texts=["直したのに"])))

        assert result["rows"][0]["state"] == "regression"
        assert result["to_judge"] == 1

    def test_the_family_label_is_echoed_onto_every_row(self, store, engine):
        engine["oracle"]["違う"] = "違 う"
        engine["cli"]["違う"] = "違う"
        row = parse_json(run(defect_probe(texts=["違う"], pattern="particle-chain")))["rows"][0]
        assert row["pattern"] == "particle-chain"

    def test_an_oversized_batch_is_refused_rather_than_truncated(self, store, engine):
        result = parse_json(run(defect_probe(texts=[f"文{idx}" for idx in range(201)])))
        assert result["status"] == "error"
        assert "200" in result["message"]

    def test_an_empty_batch_is_refused(self, store, engine):
        assert parse_json(run(defect_probe(texts=[])))["status"] == "error"


# ============================================================================
# defect_dismiss
# ============================================================================


class TestDefectDismiss:
    def _dismiss(self, **kwargs):
        params = {
            "text": "仕様準拠の文",
            "expected": "仕様 準拠 の 文",
            "suzume": "仕様準拠 の 文",
            "resolution": "conformant",
            "reason": "documented search-unit choice",
            "pattern": "compound-noun",
        }
        params.update(kwargs)
        return parse_json(run(defect_dismiss(**params)))

    def test_it_closes_without_ever_opening(self, store, engine):
        result = self._dismiss()
        assert result["status"] == "ok"
        assert bug_store.load_open("defect") == []
        assert bug_store.load_resolved("defect")[0]["resolution"] == "conformant"

    def test_fixed_is_refused_because_that_is_defect_resolve(self, store, engine):
        assert self._dismiss(resolution="fixed")["status"] == "error"

    def test_a_reason_and_a_pattern_are_both_required(self, store, engine):
        assert self._dismiss(reason=" ")["status"] == "error"
        assert self._dismiss(pattern="")["status"] == "error"
        assert bug_store.load_resolved("defect") == []

    def test_an_already_recorded_text_is_not_recorded_twice(self, store, engine):
        file_record(text="既出", expected="既 出", suzume="既出")
        assert self._dismiss(text="既出")["status"] == "duplicate"

    def test_the_outputs_are_derived_when_omitted(self, store, engine):
        engine["oracle"]["仕様準拠の文"] = "仕様 準拠 の 文"
        engine["cli"]["仕様準拠の文"] = "仕様準拠 の 文"
        self._dismiss(expected="", suzume="")
        assert bug_store.load_resolved("defect")[0]["expected"] == "仕様 準拠 の 文"

    def test_reopening_puts_it_back_for_another_look(self, store, engine):
        self._dismiss()
        run(defect_reopen(ids="1"))
        assert len(bug_store.load_open("defect")) == 1


# ============================================================================
# defect_yield
# ============================================================================


class TestDefectYield:
    def test_it_reports_the_hit_rate_per_family(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab", pattern="particle-chain")
        run(
            defect_dismiss(
                text="b",
                expected="c d",
                suzume="cd",
                resolution="ambiguous",
                reason="two valid readings",
                pattern="particle-chain",
            )
        )

        result = parse_json(run(defect_yield()))

        family = result["families"]["particle-chain"]
        assert family["judged"] == 2
        assert family["filed"] == 1
        assert family["dismissed"] == 1
        assert family["hit_rate"] == 0.5
        assert result["totals"]["judged"] == 2

    def test_thin_families_can_be_hidden(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab", pattern="thin")
        file_record(text="b", expected="c d", suzume="cd", pattern="thick")
        file_record(text="c", expected="e f", suzume="ef", pattern="thick")

        result = parse_json(run(defect_yield(min_judged=2)))

        assert list(result["families"]) == ["thick"]

    def test_an_empty_store_reports_no_families(self, store, engine):
        result = parse_json(run(defect_yield()))
        assert result["families"] == {}
        assert result["totals"]["hit_rate"] == 0.0

    def test_a_matching_probe_updates_its_family_last_probed_time(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab", pattern="particle-chain")

        result = parse_json(run(defect_probe(texts=["no-diff"], pattern="particle-chain")))
        families = parse_json(run(defect_yield()))["families"]

        assert result["counts"] == {"match": 1}
        assert "T" in families["particle-chain"]["last_probed"]


# ============================================================================
# kind
# ============================================================================


class TestKind:
    def test_the_side_at_fault_is_recorded(self, store, engine):
        result = parse_json(run(defect_add(text="a", expected="a b", suzume="ab", kind="oracle")))
        assert result["kind"] == "oracle"
        assert bug_store.load_open("defect")[0]["kind"] == "oracle"

    def test_an_unknown_side_is_refused(self, store, engine):
        assert parse_json(run(defect_add(text="a", expected="a b", suzume="ab", kind="mecab")))["status"] == "error"
        assert bug_store.load_open("defect") == []

    def test_records_can_be_listed_by_side(self, store, engine):
        file_record(text="a", expected="a b", suzume="ab")
        file_record(text="b", expected="c d", suzume="cd", kind="oracle")

        result = parse_json(run(defect_list(kind="oracle")))

        assert result["matched"] == 1
        assert result["records"][0]["kind"] == "oracle"

    def test_the_side_does_not_change_how_a_record_is_closed(self, store, engine):
        file_record(text="直った", expected="直 った", suzume="直った", kind="oracle")
        engine["oracle"]["直った"] = "直 った"
        engine["cli"]["直った"] = "直 った"

        assert parse_json(run(defect_recheck()))["counts"] == {"resolved": 1}


class TestListStatus:
    def test_closed_records_can_be_listed(self, store, engine):
        record = file_record(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        file_record(text="b", expected="c d", suzume="cd")

        assert parse_json(run(defect_list()))["matched"] == 1
        assert parse_json(run(defect_list(status="resolved")))["matched"] == 1
        assert parse_json(run(defect_list(status="all")))["matched"] == 2

    def test_all_status_is_id_ordered_and_compact_rows_identify_status(self, store, engine):
        closed = file_record(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", closed, note="fixed", output="a b")
        file_record(text="b", expected="c d", suzume="cd")

        records = parse_json(run(defect_list(status="all")))["records"]
        assert [(record["id"], record["status"]) for record in records] == [(1, "resolved"), (2, "open")]

    def test_filtering_by_resolution_implies_the_closed_set(self, store, engine):
        run(
            defect_dismiss(
                text="a",
                expected="a b",
                suzume="ab",
                resolution="known-limit",
                reason="documented trade-off",
                pattern="suffix",
            )
        )
        record = file_record(text="b", expected="c d", suzume="cd")
        bug_store.resolve("defect", record, note="fixed", output="c d")

        result = parse_json(run(defect_list(resolution="known-limit")))

        assert result["matched"] == 1
        assert result["records"][0]["text"] == "a"
