"""Tests for the file-backed defect store."""

import json

import pytest

from suzume_mcp.core import bug_store


@pytest.fixture
def store(tmp_path, monkeypatch):
    """Point the "defect" source at a temporary directory."""
    path = tmp_path / "bugs"
    monkeypatch.setitem(bug_store._SOURCE_DIRS, "defect", path)
    return path


ORACLE_READING = [{"surface": "ab"}]


def make(store_name="defect", **kwargs):
    params = {
        "text": "テスト文",
        "expected": "テスト 文",
        "suzume": "テスト文",
    }
    params.update(kwargs)
    return bug_store.create(store_name, **params)


# ============================================================================
# Token normalization
# ============================================================================


class TestTokenNormalization:
    @pytest.mark.parametrize(
        ("raw", "expected"),
        [
            ("女らしい / 服装", ["女らしい", "服装"]),
            ("女らしい 服装", ["女らしい", "服装"]),
            ("  a/b   c ", ["a", "b", "c"]),
            ("", []),
        ],
    )
    def test_both_separators_parse_the_same(self, raw, expected):
        assert bug_store.normalize_tokens(raw) == expected

    def test_canonical_form_is_space_separated(self):
        assert bug_store.canonical_tokens("女らしい / 服装") == "女らしい 服装"

    def test_identical_token_lists_mean_a_pos_only_record(self):
        assert bug_store.detect_check("あ い", "あ / い") == bug_store.CHECK_MANUAL
        assert bug_store.detect_check("あ い", "あい") == bug_store.CHECK_SURFACE


# ============================================================================
# Numbering
# ============================================================================


class TestNextId:
    def test_empty_store_starts_at_one(self, store):
        assert bug_store.next_id("defect") == 1

    def test_numbering_crosses_the_digit_boundary(self, store):
        """Numbering must not regress when ids gain a digit.

        File names sort lexically, so "1000_x.json" precedes "999_x.json". Reading
        the id field instead keeps the sequence monotonic.
        """
        record = make(text="a", expected="a b", suzume="ab")
        record["id"] = 998
        bug_store.write_record("defect", record)

        assert bug_store.next_id("defect") == 999
        make(text="b", expected="c d", suzume="cd")  # 999
        assert bug_store.next_id("defect") == 1000
        make(text="c", expected="e f", suzume="ef")  # 1000
        assert bug_store.next_id("defect") == 1001
        make(text="d", expected="g h", suzume="gh")  # 1001
        assert bug_store.next_id("defect") == 1002
        assert sorted(rec["id"] for rec in bug_store.load_open("defect")) == [998, 999, 1000, 1001]

    def test_resolved_records_still_reserve_their_id(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        assert bug_store.load_open("defect") == []
        assert bug_store.next_id("defect") == record["id"] + 1

    def test_archived_records_still_reserve_their_id(self, store):
        make(text="a", expected="a b", suzume="ab")
        bug_store.archive("defect", "2026-07-28-120000")
        assert bug_store.next_id("defect") == 2

    def test_a_record_written_by_an_older_tool_is_counted(self, store):
        store.mkdir(parents=True)
        (store / "007_boundary.json").write_text(
            json.dumps({"text": "x", "expected": "a b", "suzume": "ab"}, ensure_ascii=False),
            encoding="utf-8",
        )
        assert bug_store.next_id("defect") == 8


# ============================================================================
# Reading
# ============================================================================


class TestLoad:
    def test_defaults_are_filled_in_for_an_older_record(self, store):
        store.mkdir(parents=True)
        (store / "003_over-split.json").write_text(
            json.dumps(
                {"id": 3, "text": "女らしい服装", "expected": "女らしい / 服装", "suzume": "女 / らしい / 服装"},
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        record = bug_store.load_open("defect")[0]
        assert record["expected"] == "女らしい 服装"
        assert record["suzume"] == "女 らしい 服装"
        assert record["check"] == bug_store.CHECK_SURFACE
        assert record["priority"] == ""
        assert record["status"] == "open"

    def test_sub_directories_are_never_scanned(self, store):
        make(text="a", expected="a b", suzume="ab")
        for name in (bug_store.RESOLVED_DIRNAME, bug_store.ARCHIVE_DIRNAME, "backup"):
            nested = store / name
            nested.mkdir(parents=True)
            (nested / "0500_boundary.json").write_text(
                json.dumps({"id": 500, "text": "nested"}, ensure_ascii=False), encoding="utf-8"
            )

        assert [rec["id"] for rec in bug_store.load_open("defect")] == [1]

    def test_a_corrupt_file_blocks_the_listing_and_names_the_file(self, store):
        make(text="a", expected="a b", suzume="ab")
        (store / "0002_broken.json").write_text("{not json", encoding="utf-8")
        with pytest.raises(bug_store.BugStoreError, match=r"0002_broken\.json"):
            bug_store.load_open("defect")

    def test_lookup_by_text_covers_resolved_records(self, store):
        record = make(text="同じ文", expected="同じ 文", suzume="同じ文")
        bug_store.resolve("defect", record, note="fixed", output="同じ 文")
        assert bug_store.find_by_text("defect", "同じ文")["id"] == record["id"]
        assert bug_store.find_by_text("defect", "同じ文", include_resolved=False) is None


# ============================================================================
# Writing and retirement
# ============================================================================


class TestCreate:
    def test_fields_are_derived_and_canonicalized(self, store):
        record = make(text="女らしい服装", expected="女らしい / 服装", suzume="女 / らしい / 服装", priority="HIGH")
        assert record["id"] == 1
        assert record["expected"] == "女らしい 服装"
        assert record["diff_type"] == "over-split"
        assert record["check"] == bug_store.CHECK_SURFACE
        assert record["priority"] == "high"
        assert record["_file"] == "0001_over-split.json"

    def test_a_pos_only_record_is_marked_manual(self, store):
        record = make(text="本の多く", expected="本 の 多く", suzume="本 の 多く")
        assert record["check"] == bug_store.CHECK_MANUAL

    def test_file_name_is_zero_padded_to_four_digits(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        record["id"] = 1234
        path = bug_store.write_record("defect", record)
        assert path.name == "1234_under-split.json"

    def test_record_writes_do_not_use_path_write_text(self, store, monkeypatch):
        record = make(text="a", expected="a b", suzume="ab")

        def fail(*_args, **_kwargs):
            raise AssertionError("non-atomic Path.write_text used")

        monkeypatch.setattr(type(store), "write_text", fail)
        bug_store.write_record("defect", record)
        assert (store / record["_file"]).read_text(encoding="utf-8")


class TestUpdate:
    def test_changed_fields_are_persisted(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.update("defect", record, {"pattern": "compound-verb", "priority": "low"})
        reloaded = bug_store.load_open("defect")[0]
        assert reloaded["pattern"] == "compound-verb"
        assert reloaded["priority"] == "low"

    def test_a_renamed_file_does_not_leave_a_second_copy(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.update("defect", record, {"diff_type": "boundary"})
        assert [path.name for path in sorted(store.glob("*.json"))] == ["0001_boundary.json"]


class TestResolveAndReopen:
    def test_resolving_moves_the_record_instead_of_deleting_it(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="plan p1 package 3", output="a b")

        assert bug_store.load_open("defect") == []
        retired = bug_store.load_resolved("defect")
        assert len(retired) == 1
        assert retired[0]["status"] == "resolved"
        assert retired[0]["resolved_note"] == "plan p1 package 3"
        assert retired[0]["resolved_output"] == "a b"
        assert retired[0]["resolved_at"]

    def test_reopening_restores_the_record_and_clears_its_closure(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        bug_store.reopen("defect", bug_store.load_resolved("defect")[0])

        assert bug_store.load_resolved("defect") == []
        reopened = bug_store.load_open("defect")[0]
        assert reopened["status"] == "open"
        assert "resolved_at" not in reopened
        assert "resolved_note" not in reopened


class TestArchive:
    def test_records_are_moved_into_a_timestamped_directory(self, store):
        make(text="a", expected="a b", suzume="ab")
        make(text="b", expected="c d", suzume="cd")

        target, count = bug_store.archive("defect", "2026-07-28-120000")

        assert count == 2
        assert bug_store.load_open("defect") == []
        assert len(list(target.glob("*.json"))) == 2

    def test_retired_records_are_left_alone(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.resolve("defect", record, note="fixed", output="a b")
        make(text="b", expected="c d", suzume="cd")

        bug_store.archive("defect", "2026-07-28-120000")

        assert len(bug_store.load_resolved("defect")) == 1


# ============================================================================
# Report rendering
# ============================================================================


class TestRenderReport:
    def _records(self, store):
        make(text="女らしい服装", expected="女らしい 服装", suzume="女 らしい 服装", pattern="derived", priority="high")
        make(text="本の多く", expected="本 の 多く", suzume="本 の 多く", pattern="deverbal", priority="medium")
        return bug_store.load_open("defect")

    def test_output_is_identical_across_runs(self, store):
        records = self._records(store)
        live = {rec["id"]: {"state": "open", "current": rec["suzume"]} for rec in records}
        first = bug_store.render_report("defect", records, live, 0, "2026-07-28")
        second = bug_store.render_report("defect", list(reversed(records)), live, 0, "2026-07-28")
        assert first == second

    def test_current_output_comes_from_the_live_run(self, store):
        records = self._records(store)
        live = {records[0]["id"]: {"state": "stale", "current": "女 らしい 服 装"}}
        report = bug_store.render_report("defect", records, live, 0, "2026-07-28")
        assert "女 らしい 服 装" in report
        assert "| stale | 1 | #1 |" in report

    def test_every_state_is_listed_with_its_ids(self, store):
        records = self._records(store)
        live = {
            records[0]["id"]: {"state": "resolved", "current": "女らしい 服装"},
            records[1]["id"]: {"state": "needs-manual", "current": "本 の 多く"},
        }
        report = bug_store.render_report("defect", records, live, 0, "2026-07-28")
        assert "| resolved | 1 | #1 |" in report
        assert "| needs-manual | 1 | #2 |" in report

    def test_a_record_with_no_live_result_counts_as_open(self, store):
        records = self._records(store)
        report = bug_store.render_report("defect", records, {}, 0, "2026-07-28")
        assert "| open | 2 |" in report

    def test_singleton_patterns_are_summarized_as_a_count(self, store):
        self._records(store)
        make(text="c", expected="a b", suzume="ab", pattern="derived")
        records = bug_store.load_open("defect")
        report = bug_store.render_report("defect", records, {}, 0, "2026-07-28")
        assert "| patterns holding more than one record | derived 2 |" in report
        assert "| singleton patterns | 1 |" in report

    def test_pos_only_records_are_flagged(self, store):
        records = self._records(store)
        report = bug_store.render_report("defect", records, {}, 0, "2026-07-28")
        assert "**m**" in report

    def test_a_pipe_in_a_field_does_not_break_the_table(self, store):
        make(text="a|b", expected="a b", suzume="ab", description="note | with pipe")
        records = bug_store.load_open("defect")
        report = bug_store.render_report("defect", records, {}, 0, "2026-07-28")
        assert "a\\|b" in report
        assert "note \\| with pipe" in report


class TestSummarize:
    def test_counts_cover_every_axis(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="p1", priority="high")
        make(text="b", expected="c d", suzume="cd", pattern="p1", priority="low")
        make(text="c", expected="e f", suzume="e f", pattern="p2")

        summary = bug_store.summarize(bug_store.load_open("defect"))

        assert summary["total"] == 3
        assert summary["patterns"] == {"p1": 2, "p2": 1}
        assert summary["priorities"]["high"] == 1
        assert summary["manual"] == 1


class TestStoreDir:
    def test_defect_does_not_take_the_quality_check_suffix(self):
        assert bug_store.store_dir("defect").name == "bugs"
        assert bug_store.store_dir("defect").parent.name == "defect-sweep"

    def test_an_unknown_source_is_rejected(self):
        with pytest.raises(bug_store.BugStoreError, match="Unknown source"):
            bug_store.store_dir("custom")

    def test_a_path_like_source_is_rejected(self):
        with pytest.raises(bug_store.BugStoreError):
            bug_store.store_dir("../../etc")


# ============================================================================
# kind: which side of the comparison is wrong
# ============================================================================


class TestKind:
    def test_a_record_defaults_to_a_tokenizer_defect(self, store):
        assert make(text="a", expected="a b", suzume="ab")["kind"] == bug_store.KIND_TOKENIZER

    def test_an_oracle_defect_is_recorded_as_such(self, store):
        record = make(text="a", expected="a b", suzume="ab", kind=bug_store.KIND_ORACLE, corrected=ORACLE_READING)
        assert record["kind"] == bug_store.KIND_ORACLE
        assert bug_store.load_open("defect")[0]["kind"] == bug_store.KIND_ORACLE

    def test_a_record_written_before_kind_existed_reads_as_tokenizer(self, store):
        store.mkdir(parents=True)
        (store / "0003_over-split.json").write_text(
            json.dumps({"id": 3, "text": "x", "expected": "a b", "suzume": "ab"}, ensure_ascii=False),
            encoding="utf-8",
        )
        assert bug_store.load_open("defect")[0]["kind"] == bug_store.KIND_TOKENIZER

    def test_the_summary_counts_each_side(self, store):
        make(text="a", expected="a b", suzume="ab")
        make(text="b", expected="c d", suzume="cd", kind=bug_store.KIND_ORACLE, corrected=ORACLE_READING)
        summary = bug_store.summarize(bug_store.load_open("defect"))
        assert summary["kinds"] == {"oracle": 1, "tokenizer": 1}


# ============================================================================
# Dismissal: the store doubles as the ledger of judged non-defects
# ============================================================================


def dismiss(**kwargs):
    params = {
        "text": "判定済みの文",
        "expected": "判定 済み の 文",
        "suzume": "判定済み の 文",
        "resolution": "conformant",
        "reason": "search-unit choice, documented",
        "pattern": "compound-noun",
    }
    params.update(kwargs)
    return bug_store.dismiss("defect", **params)


class TestDismiss:
    def test_it_lands_in_resolved_without_ever_being_open(self, store):
        record = dismiss()
        assert bug_store.load_open("defect") == []
        retired = bug_store.load_resolved("defect")
        assert len(retired) == 1
        assert retired[0]["resolution"] == "conformant"
        assert retired[0]["resolved_note"] == "search-unit choice, documented"
        assert record["id"] == 1

    @pytest.mark.parametrize("resolution", ["conformant", "known-limit", "ambiguous", "duplicate"])
    def test_every_non_fixed_resolution_is_accepted(self, store, resolution):
        assert dismiss(text=f"文{resolution}", resolution=resolution)["resolution"] == resolution

    def test_fixed_belongs_to_resolve_not_here(self, store):
        with pytest.raises(bug_store.BugStoreError):
            dismiss(resolution=bug_store.RESOLUTION_FIXED)

    def test_an_unknown_resolution_is_rejected(self, store):
        with pytest.raises(bug_store.BugStoreError):
            dismiss(resolution="whatever")

    def test_a_reason_is_required(self, store):
        with pytest.raises(bug_store.BugStoreError):
            dismiss(reason="  ")

    def test_a_pattern_is_required_because_it_is_the_yield_denominator(self, store):
        with pytest.raises(bug_store.BugStoreError):
            dismiss(pattern="")

    def test_reopening_a_dismissal_clears_its_resolution(self, store):
        dismiss()
        bug_store.reopen("defect", bug_store.load_resolved("defect")[0])
        reopened = bug_store.load_open("defect")[0]
        assert "resolution" not in reopened
        assert reopened["status"] == "open"


# ============================================================================
# Yield: where to spend the next round
# ============================================================================


class TestYieldByPattern:
    def test_both_outcomes_of_a_judgment_form_the_denominator(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="particles")
        dismiss(text="b", pattern="particles")
        dismiss(text="c", pattern="particles", resolution="ambiguous")

        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), bug_store.load_resolved("defect"))

        assert families["particles"]["judged"] == 3
        assert families["particles"]["filed"] == 1
        assert families["particles"]["dismissed"] == 2
        assert families["particles"]["hit_rate"] == round(1 / 3, 3)

    def test_a_record_closed_as_fixed_still_counts_as_a_find(self, store):
        record = make(text="a", expected="a b", suzume="ab", pattern="colloquial")
        bug_store.resolve("defect", record, note="fixed", output="a b")

        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), bug_store.load_resolved("defect"))

        assert families["colloquial"] == {
            "judged": 1,
            "filed": 1,
            "open": 0,
            "dismissed": 0,
            "kinds": {"tokenizer": 1},
            "resolutions": {"fixed": 1},
            "last_probed": bug_store.today(),
            "hit_rate": 1.0,
        }

    def test_families_sort_by_hit_rate(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="productive")
        dismiss(text="b", pattern="mined-out")
        dismiss(text="c", pattern="mined-out", resolution="known-limit")

        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), bug_store.load_resolved("defect"))

        assert list(families) == ["productive", "mined-out"]
        assert families["mined-out"]["hit_rate"] == 0.0

    def test_an_untagged_record_collects_under_a_single_bucket(self, store):
        make(text="a", expected="a b", suzume="ab")
        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), [])
        assert families["(none)"]["judged"] == 1

    def test_an_empty_store_produces_no_families(self, store):
        assert bug_store.yield_by_pattern([], []) == {}

    def test_the_oracle_side_is_visible_per_family(self, store):
        make(
            text="a",
            expected="a b",
            suzume="ab",
            pattern="literary",
            kind=bug_store.KIND_ORACLE,
            corrected=ORACLE_READING,
        )
        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), [])
        assert families["literary"]["kinds"] == {"oracle": 1}

    def test_a_completed_probe_refreshes_the_family_scheduler_timestamp(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="particles")
        timestamp = f"{bug_store.today()}T12:00:00+09:00"
        bug_store.record_probe("defect", "particles", timestamp=timestamp)

        families = bug_store.yield_by_pattern(bug_store.load_open("defect"), [], source="defect")

        assert families["particles"]["last_probed"] == timestamp


class TestReportKind:
    def test_oracle_records_get_their_own_section(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="p1")
        make(text="b", expected="c d", suzume="cd", pattern="p2", kind=bug_store.KIND_ORACLE, corrected=ORACLE_READING)
        report = bug_store.render_report("defect", bug_store.load_open("defect"), {}, 0, "2026-07-28")
        assert "## Oracle-side records" in report
        assert "| kind | oracle 1 / tokenizer 1 |" in report

    def test_a_tokenizer_only_store_omits_the_oracle_section(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="p1")
        report = bug_store.render_report("defect", bug_store.load_open("defect"), {}, 0, "2026-07-28")
        assert "Oracle-side records" not in report

    def test_output_stays_deterministic_with_kinds(self, store):
        make(text="a", expected="a b", suzume="ab", pattern="p1", kind=bug_store.KIND_ORACLE, corrected=ORACLE_READING)
        make(text="b", expected="c d", suzume="cd", pattern="p0")
        records = bug_store.load_open("defect")
        first = bug_store.render_report("defect", records, {}, 0, "2026-07-28")
        second = bug_store.render_report("defect", list(reversed(records)), {}, 0, "2026-07-28")
        assert first == second


# ============================================================================
# Corrected reading: the ground truth an oracle-side record closes against
# ============================================================================


class TestParseCorrected:
    def test_a_full_reading_carries_pos_and_lemma(self):
        assert bug_store.parse_corrected("よい|Adjective はなし|Noun|はなし です|Auxiliary|です") == [
            {"surface": "よい", "pos": "Adjective", "lemma": "よい"},
            {"surface": "はなし", "pos": "Noun", "lemma": "はなし"},
            {"surface": "です", "pos": "Auxiliary", "lemma": "です"},
        ]

    def test_an_omitted_lemma_stands_for_the_surface(self):
        assert bug_store.parse_corrected("行っ|Verb|行く た|Auxiliary")[1]["lemma"] == "た"

    def test_a_surface_only_reading_is_valid(self):
        assert bug_store.parse_corrected("これ は ちゅうしゃ だ") == [
            {"surface": "これ"},
            {"surface": "は"},
            {"surface": "ちゅうしゃ"},
            {"surface": "だ"},
        ]

    @pytest.mark.parametrize(
        "raw",
        [
            "",
            "   ",
            "よい|Adjective はなし",  # mixes full tokens with surface-only ones
            "よい|ADJ",  # not a canonical POS
            "よい|Adjective|よい|extra",
            "|Noun",
        ],
    )
    def test_malformed_readings_are_rejected(self, raw):
        with pytest.raises(bug_store.BugStoreError):
            bug_store.parse_corrected(raw)

    @pytest.mark.parametrize("raw", ["よい|Adjective|よい はなし|Noun|はなし", "これ は ちゅうしゃ"])
    def test_format_round_trips(self, raw):
        assert bug_store.format_corrected(bug_store.parse_corrected(raw)) == raw


class TestCorrectedOnRecords:
    @pytest.mark.parametrize("kind", [bug_store.KIND_ORACLE, bug_store.KIND_BOTH])
    def test_an_oracle_side_record_requires_a_corrected_reading(self, store, kind):
        with pytest.raises(bug_store.BugStoreError, match="corrected"):
            make(text="a", expected="a b", suzume="ab", kind=kind)

    def test_the_reading_is_persisted(self, store):
        reading = bug_store.parse_corrected("a|Noun b|Particle")
        make(text="a", expected="ab", suzume="ab", kind=bug_store.KIND_ORACLE, corrected=reading)
        assert bug_store.load_open("defect")[0]["corrected"] == [
            {"surface": "a", "pos": "Noun", "lemma": "a"},
            {"surface": "b", "pos": "Particle", "lemma": "b"},
        ]

    def test_a_tokenizer_record_needs_none(self, store):
        assert "corrected" not in make(text="a", expected="a b", suzume="ab")

    def test_turning_a_record_oracle_side_without_a_reading_is_refused(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        with pytest.raises(bug_store.BugStoreError, match="corrected"):
            bug_store.update("defect", record, {"kind": bug_store.KIND_ORACLE})
        assert bug_store.load_open("defect")[0]["kind"] == bug_store.KIND_TOKENIZER

    def test_a_reading_can_be_added_with_the_kind(self, store):
        record = make(text="a", expected="a b", suzume="ab")
        bug_store.update("defect", record, {"kind": bug_store.KIND_BOTH, "corrected": ORACLE_READING})
        reloaded = bug_store.load_open("defect")[0]
        assert (reloaded["kind"], reloaded["corrected"]) == (bug_store.KIND_BOTH, ORACLE_READING)

    def test_a_stored_record_without_a_reading_still_loads(self, store):
        store.mkdir(parents=True)
        (store / "0004_over-split.json").write_text(
            json.dumps({"id": 4, "text": "x", "expected": "a b", "suzume": "ab", "kind": "oracle"}),
            encoding="utf-8",
        )
        assert "corrected" not in bug_store.load_open("defect")[0]

    def test_the_report_shows_the_reading(self, store):
        reading = bug_store.parse_corrected("a|Noun b|Particle")
        make(text="ab", expected="ab", suzume="a b", kind=bug_store.KIND_ORACLE, corrected=reading)
        report = bug_store.render_report("defect", bug_store.load_open("defect"), {}, 0, "2026-07-28")
        assert "| corrected |" in report
        assert "a\\|Noun\\|a b\\|Particle\\|b" in report
