#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string_view>
#include <vector>

// Include the header directly since we add the CLI source dir to includes
#include "dict_compiler.h"
#include "dictionary/binary_dict.h"

namespace suzume::cli {
namespace {

class DictCompilerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("suzume_dict_compiler_test_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  std::filesystem::path writeFile(const std::string& name, const std::string& content) const {
    auto path = temp_dir_ / name;
    std::ofstream file(path);
    file << content;
    return path;
  }

  std::filesystem::path temp_dir_;
};

// Pure kanji 3+ chars: trivial (can be segmented by rules)
TEST(IsTrivialEntryTest, PureKanjiThreeChars) {
  EXPECT_TRUE(isTrivialEntry("経済成長"));
  EXPECT_TRUE(isTrivialEntry("東京都"));
  EXPECT_TRUE(isTrivialEntry("形態素解析"));
}

// Pure katakana 3+ chars: trivial
TEST(IsTrivialEntryTest, PureKatakanaThreeChars) {
  EXPECT_TRUE(isTrivialEntry("テスト"));
  EXPECT_TRUE(isTrivialEntry("コンピュータ"));
  EXPECT_TRUE(isTrivialEntry("プログラム"));
}

// Pure hiragana: non-trivial. Hiragana runs carry lexical value and are not
// reconstructable from character type alone, so they are kept.
TEST(IsTrivialEntryTest, PureHiraganaThreeChars) {
  EXPECT_FALSE(isTrivialEntry("ありがとう"));
  EXPECT_FALSE(isTrivialEntry("こんにちは"));
}

// 2-char entries: always non-trivial (kept)
TEST(IsTrivialEntryTest, TwoCharEntries) {
  EXPECT_FALSE(isTrivialEntry("東京"));
  EXPECT_FALSE(isTrivialEntry("りん"));
  EXPECT_FALSE(isTrivialEntry("AB"));
}

// 1-char entries: non-trivial (kept)
TEST(IsTrivialEntryTest, SingleCharEntries) {
  EXPECT_FALSE(isTrivialEntry("東"));
  EXPECT_FALSE(isTrivialEntry("A"));
}

// Mixed kanji+katakana: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedKanjiKatakana) {
  EXPECT_FALSE(isTrivialEntry("二次データ"));
  EXPECT_FALSE(isTrivialEntry("東京データ"));
}

// Mixed kanji+hiragana: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedKanjiHiragana) {
  EXPECT_FALSE(isTrivialEntry("掘り出し物"));
  EXPECT_FALSE(isTrivialEntry("食べ物"));
}

// Entries with spaces: always non-trivial (kept)
TEST(IsTrivialEntryTest, EntriesWithSpaces) {
  EXPECT_FALSE(isTrivialEntry("東京 都"));
  EXPECT_FALSE(isTrivialEntry("テスト ケース"));
}

// Empty string: non-trivial (0 codepoints <= 2)
TEST(IsTrivialEntryTest, EmptyString) {
  EXPECT_FALSE(isTrivialEntry(""));
}

// Pure alphabet: non-trivial (only katakana/kanji runs are reconstructable).
TEST(IsTrivialEntryTest, PureAlphabetThreeChars) {
  EXPECT_FALSE(isTrivialEntry("ABC"));
  EXPECT_FALSE(isTrivialEntry("test"));
}

// Mixed alphabet+digit: non-trivial (kept)
TEST(IsTrivialEntryTest, MixedAlphabetDigit) {
  EXPECT_FALSE(isTrivialEntry("part3"));
  EXPECT_FALSE(isTrivialEntry("ABC123"));
}

TEST_F(DictCompilerTest, CompileMultipleRejectsDuplicateSurfaceAndPosAcrossFiles) {
  auto first = writeFile("first.tsv", "東京\tNOUN\n");
  auto second = writeFile("second.tsv", "東京\tNOUN\n");
  auto output = temp_dir_ / "out.dic";

  DictCompiler compiler;
  auto result = compiler.compileMultiple({first.string(), second.string()}, output.string());

  EXPECT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Validation failed"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(output));
}

TEST_F(DictCompilerTest, CompileMultiplePreservesDifferentPosForSameSurface) {
  auto noun = writeFile("nouns.tsv", "最悪\tNOUN\n");
  auto adjective = writeFile("adjectives.tsv", "最悪\tADJECTIVE\tNA_ADJ\n");
  auto output = temp_dir_ / "out.dic";

  DictCompiler compiler;
  auto result = compiler.compileMultiple({noun.string(), adjective.string()}, output.string());
  ASSERT_TRUE(result.hasValue()) << result.error().message;

  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(output.string());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;
  EXPECT_NE(dict.lookupExact("最悪", core::PartOfSpeech::Noun), nullptr);
  EXPECT_NE(dict.lookupExact("最悪", core::PartOfSpeech::Adjective), nullptr);
}

TEST_F(DictCompilerTest, ExplicitSourceEntryReplacesEarlierGeneratedSurfaceCollision) {
  auto input = writeFile("explicit.tsv", "扱う\tVERB\tGODAN_WA\n扱い\tADJECTIVE\tI_ADJ\n");
  auto output = temp_dir_ / "explicit.dic";

  DictCompiler compiler;
  auto result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(result.hasValue()) << result.error().message;

  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(output.string());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;
  const auto matches = dict.lookup("扱い", 0);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].entry->pos, core::PartOfSpeech::Adjective);
  EXPECT_EQ(matches[0].entry->lemma, "扱い");
}

TEST_F(DictCompilerTest, GeneratedSamePosCollisionKeepsLongerLemmaEvidence) {
  auto input = writeFile("generated.tsv", "届く\tVERB\tGODAN_KA\n届ける\tVERB\tICHIDAN\n");
  auto output = temp_dir_ / "generated.dic";

  DictCompiler compiler;
  auto result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(result.hasValue()) << result.error().message;

  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(output.string());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;
  const auto matches = dict.lookup("届け", 0);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].entry->lemma, "届ける");
}

TEST_F(DictCompilerTest, KuruExpansionGeneratesSafeKanjiAndKanaSurfaces) {
  auto input = writeFile("kuru.tsv", "来る\tVERB\tKURU\n");
  auto output = temp_dir_ / "kuru.dic";

  DictCompiler compiler;
  auto compile_result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(compile_result.hasValue()) << compile_result.error().message;

  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(output.string());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;

  std::set<std::string> surfaces;
  for (std::string_view text :
       {"来る", "来", "来れ", "来よ", "来い", "来られる", "来れる", "くる", "くれ", "こよ", "こい"}) {
    for (const auto& result : dict.lookup(text, 0)) {
      ASSERT_NE(result.entry, nullptr);
      surfaces.insert(result.entry->surface);
      const bool kana_surface = result.entry->surface != "来る" && result.entry->surface != "来" &&
                                result.entry->surface != "来れ" && result.entry->surface != "来よ" &&
                                result.entry->surface != "来い" && result.entry->surface != "来られる" &&
                                result.entry->surface != "来れる";
      EXPECT_EQ(result.entry->lemma, kana_surface ? "くる" : "来る");
    }
  }

  const auto has_surface = [&surfaces](std::string_view surface) {
    return surfaces.find(std::string(surface)) != surfaces.end();
  };

  EXPECT_TRUE(has_surface("来る"));
  EXPECT_TRUE(has_surface("来"));
  EXPECT_TRUE(has_surface("来れ"));
  EXPECT_TRUE(has_surface("来よ"));
  EXPECT_TRUE(has_surface("来い"));
  // The passive/potential is the irrealis 来 plus られる, like the causative.
  EXPECT_FALSE(has_surface("来られる"));
  EXPECT_TRUE(has_surface("来れる"));
  EXPECT_TRUE(has_surface("くる"));
  EXPECT_TRUE(has_surface("くれ"));
  EXPECT_TRUE(has_surface("こよ"));
  EXPECT_TRUE(has_surface("こい"));
  EXPECT_FALSE(has_surface("き"));
  EXPECT_FALSE(has_surface("こ"));
  EXPECT_FALSE(has_surface("こられる"));
  EXPECT_FALSE(has_surface("これる"));
  EXPECT_FALSE(has_surface("こさせる"));
  EXPECT_FALSE(has_surface("来くる"));
  EXPECT_FALSE(has_surface("来き"));
  EXPECT_FALSE(has_surface("来こ"));
}

TEST_F(DictCompilerTest, CompileRejectsConjugationTypeSurfaceMismatch) {
  auto input = writeFile("invalid_suru.tsv", "テスト\tVERB\tSURU\n");
  auto output = temp_dir_ / "invalid_suru.dic";

  DictCompiler compiler;
  const auto result = compiler.compile(input.string(), output.string());

  ASSERT_FALSE(result.hasValue());
  EXPECT_NE(result.error().message.find("Validation failed"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(output));
}

TEST_F(DictCompilerTest, KuruCompoundExpansionRetainsPrefixAndLemma) {
  auto input = writeFile("compound_kuru.tsv", "持って来る\tVERB\tKURU\n");
  auto output = temp_dir_ / "compound_kuru.dic";

  DictCompiler compiler;
  const auto compile_result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(compile_result.hasValue()) << compile_result.error().message;

  dictionary::BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromFile(output.string()).hasValue());
  const auto* base = dict.lookupExact("持って来る", core::PartOfSpeech::Verb);
  const auto* conditional = dict.lookupExact("持って来れ", core::PartOfSpeech::Verb);
  const auto* kana_base = dict.lookupExact("持ってくる", core::PartOfSpeech::Verb);
  ASSERT_NE(base, nullptr);
  ASSERT_NE(conditional, nullptr);
  ASSERT_NE(kana_base, nullptr);
  EXPECT_EQ(base->lemma, "持って来る");
  EXPECT_EQ(conditional->lemma, "持って来る");
  EXPECT_EQ(kana_base->lemma, "持ってくる");
  EXPECT_EQ(dict.lookupExact("来る", core::PartOfSpeech::Verb), nullptr);
}

TEST_F(DictCompilerTest, IkuExpansionUsesSokuonbinWithoutShadowingGodanWaRenyokei) {
  auto input = writeFile("iku.tsv",
                         "行く\tVERB\tGODAN_KA\n行う\tVERB\tGODAN_WA\n"
                         "いく\tVERB\tGODAN_KA\nいう\tVERB\tGODAN_WA\n");
  auto output = temp_dir_ / "iku.dic";

  DictCompiler compiler;
  auto compile_result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(compile_result.hasValue()) << compile_result.error().message;

  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(output.string());
  ASSERT_TRUE(load_result.hasValue()) << load_result.error().message;

  const auto lemmas_for = [&dict](std::string_view surface) {
    std::set<std::string> lemmas;
    for (const auto& result : dict.lookup(surface, 0)) {
      if (result.entry != nullptr && result.entry->surface == surface) {
        lemmas.insert(result.entry->lemma);
      }
    }
    return lemmas;
  };

  EXPECT_EQ(lemmas_for("行っ"), std::set<std::string>({"行く"}));
  EXPECT_EQ(lemmas_for("いっ"), std::set<std::string>({"いく"}));
  EXPECT_EQ(lemmas_for("行い"), std::set<std::string>({"行う"}));
  EXPECT_EQ(lemmas_for("いい"), std::set<std::string>({"いう"}));
}

TEST_F(DictCompilerTest, NaAdjectiveStoresSpecificExtendedPos) {
  auto input = writeFile("adjective.tsv", "穏やか\tADJECTIVE\tNA_ADJ\n");
  auto output = temp_dir_ / "adjective.dic";

  DictCompiler compiler;
  auto compile_result = compiler.compile(input.string(), output.string());
  ASSERT_TRUE(compile_result.hasValue()) << compile_result.error().message;

  dictionary::BinaryDictionary dict;
  ASSERT_TRUE(dict.loadFromFile(output.string()).hasValue());
  const auto* entry = dict.lookupExact("穏やか");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->extended_pos, core::ExtendedPOS::AdjNaAdj);
}

// A compiled dictionary holds expanded conjugations, so a decompiled dump can
// never be compiler input. The dump has to carry the information the binary
// really has, and compile() has to say why it refuses the dump.
TEST_F(DictCompilerTest, DecompiledDumpCarriesExtendedPosAndLemmaOfEachExpandedForm) {
  auto input = writeFile("verb.tsv", "黙る\tVERB\tGODAN_RA\n");
  auto output = temp_dir_ / "verb.dic";
  auto dump = temp_dir_ / "verb.dump.tsv";

  DictCompiler compiler;
  ASSERT_TRUE(compiler.compile(input.string(), output.string()).hasValue());

  auto decompile_result = compiler.decompile(output.string(), dump.string());
  ASSERT_TRUE(decompile_result.hasValue()) << decompile_result.error().message;
  EXPECT_GT(decompile_result.value(), 1U) << "compilation expands one source row into many forms";

  std::ifstream file(dump);
  ASSERT_TRUE(file.is_open());
  std::string header;
  ASSERT_TRUE(std::getline(file, header));
  EXPECT_EQ(header.rfind("# suzume dictionary dump", 0), 0U);

  bool saw_terminal_form = false;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    // surface, pos, extended_pos, lemma
    std::vector<std::string> fields;
    size_t start = 0;
    for (size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', start)) {
      fields.push_back(line.substr(start, tab - start));
      start = tab + 1;
    }
    fields.push_back(line.substr(start));

    ASSERT_EQ(fields.size(), 4U) << line;
    EXPECT_EQ(fields[1], "VERB");
    EXPECT_FALSE(fields[2].empty()) << "extended POS must survive the dump";
    EXPECT_EQ(fields[3], "黙る") << "lemma must survive the dump";
    if (fields[0] == "黙る") {
      saw_terminal_form = true;
    }
  }
  EXPECT_TRUE(saw_terminal_form);
}

TEST_F(DictCompilerTest, CompileRefusesADecompiledDumpWithAnExplanation) {
  auto input = writeFile("verb.tsv", "黙る\tVERB\tGODAN_RA\n");
  auto output = temp_dir_ / "verb.dic";
  auto dump = temp_dir_ / "verb.dump.tsv";
  auto round = temp_dir_ / "round.dic";

  DictCompiler compiler;
  ASSERT_TRUE(compiler.compile(input.string(), output.string()).hasValue());
  ASSERT_TRUE(compiler.decompile(output.string(), dump.string()).hasValue());

  auto single = compiler.compile(dump.string(), round.string());
  ASSERT_FALSE(single.hasValue());
  EXPECT_NE(single.error().message.find("decompiled dump"), std::string::npos);

  // compileMultiple takes a separate parse path and needs the same guard.
  auto multiple = compiler.compileMultiple({dump.string()}, round.string());
  ASSERT_FALSE(multiple.hasValue());
  EXPECT_NE(multiple.error().message.find("decompiled dump"), std::string::npos);

  EXPECT_FALSE(std::filesystem::exists(round));
}

}  // namespace
}  // namespace suzume::cli
