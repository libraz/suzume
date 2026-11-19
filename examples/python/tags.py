"""Extract content-word tags for indexing and search."""

from suzume import Suzume

TEXT = "東京都渋谷区で開催されたイベントに参加しました"


def main() -> None:
    with Suzume() as analyzer:
        tags = analyzer.generate_tags(TEXT)
        if not tags:
            raise RuntimeError("tag generation returned no tags")
        print("All tags:")
        for tag in tags:
            print(f"  {tag.tag} ({tag.pos})")

        noun_tags = analyzer.generate_tags(TEXT, pos_filter="NOUN", exclude_basic=True)
        if not noun_tags:
            raise RuntimeError("noun tag generation returned no tags")
        print("\nNoun tags (excluding basic):")
        for tag in noun_tags:
            print(f"  {tag.tag}")

        verb_adj_tags = analyzer.generate_tags(TEXT, pos_filter=["VERB", "ADJ"], use_base_form=True)
        print("\nVerb/Adjective tags (base form):")
        for tag in verb_adj_tags:
            print(f"  {tag.tag} ({tag.pos})")


if __name__ == "__main__":
    main()
