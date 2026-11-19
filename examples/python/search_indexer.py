"""Build an inverted index from Japanese text and search it."""

from collections import defaultdict

from suzume import Suzume

DOCUMENTS = {
    1: "東京都渋谷区で新しいカフェがオープンしました",
    2: "渋谷のカフェで美味しいコーヒーを飲みました",
    3: "新宿駅の近くにある図書館で本を読んでいます",
}


def main() -> None:
    with Suzume() as analyzer:
        index: defaultdict[str, set[int]] = defaultdict(set)
        for doc_id, text in DOCUMENTS.items():
            tags = analyzer.generate_tags(
                text, pos_filter=["NOUN", "VERB"], exclude_basic=True, min_length=2
            )
            if not tags:
                raise RuntimeError(f"tag generation returned no tags for document {doc_id}")
            for tag in tags:
                index[tag.tag].add(doc_id)

        print("Inverted index:")
        for tag, doc_ids in index.items():
            print(f"  {tag} -> {sorted(doc_ids)}")

        def search(query: str) -> list[int]:
            scores: defaultdict[int, int] = defaultdict(int)
            for tag in analyzer.generate_tags(query, pos_filter=["NOUN", "VERB"]):
                for doc_id in index.get(tag.tag, ()):
                    scores[doc_id] += 1
            return sorted(scores, key=lambda doc_id: -scores[doc_id])

        print('\nSearch "渋谷 カフェ":', search("渋谷 カフェ"))
        print('Search "図書館 読む":', search("図書館 読む"))
        if not index:
            raise RuntimeError("search index is empty")


if __name__ == "__main__":
    main()
