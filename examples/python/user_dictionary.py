"""Load custom vocabulary from an in-memory user dictionary."""

from suzume import Suzume

TEXT = "青空りんご園の案内図を作りました"


def main() -> None:
    with Suzume() as analyzer:
        print("Without user dictionary:")
        for morpheme in analyzer.analyze(TEXT):
            print(f"  {morpheme.surface} [{morpheme.pos}]")

        # TSV: surface<TAB>POS[<TAB>conj_type][<TAB>base_form]
        loaded = analyzer.load_user_dictionary("青空りんご園\tNOUN\n")
        print(f"\nDictionary loaded: {loaded}")

        print("\nWith user dictionary:")
        morphemes = analyzer.analyze(TEXT)
        if not any(morpheme.is_user_dict for morpheme in morphemes):
            raise RuntimeError("loaded user dictionary entry was not used")
        for morpheme in morphemes:
            base_form = (
                f" (base form: {morpheme.base_form})"
                if morpheme.base_form != morpheme.surface
                else ""
            )
            print(f"  {morpheme.surface} [{morpheme.pos}]{base_form}")

        # A compiled .dic file loads the same way:
        # analyzer.load_binary_dictionary(Path("user.dic").read_bytes())


if __name__ == "__main__":
    main()
