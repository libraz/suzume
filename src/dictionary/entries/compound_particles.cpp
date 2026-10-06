#include "entries_internal.h"

namespace suzume::dictionary::entries {

// A lexicalized compound particle is registered as a paradigm, not as whichever
// single form happened to be observed: the te-form and the bare continuative are
// the same closed unit (にわたって/にわたり), and a member that carries only one
// of them makes the class behave inconsistently. The polite variant is not part
// of the paradigm — ます is an auxiliary, so につきまして keeps the boundaries
// につき + まし + て.
EntrySpecRange getCompoundParticleEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Relation (関連)
      particle("について", EPOS::ParticleCase),  // beat false positive adjective candidates

      // Cause/Means (原因・手段)
      particle("によって", EPOS::ParticleCase),  // beat によっ(verb)+て split
      particle("により", EPOS::ParticleCase),
      particle("につき", EPOS::ParticleCase),    // formal reason/topic marker in notices
      particle("につれて", EPOS::ParticleCase),  // beat に+つれ(verb)+て split
      particle("につれ", EPOS::ParticleCase),
      particle("に従って", EPOS::ParticleCase),  // formal compliance/sequence marker
      particle("に従い", EPOS::ParticleCase),
      particle("にかけて", EPOS::ParticleCase),  // beat にか(noun)+けて(verb) split
      particle("による", EPOS::ParticleCase),    // adnominal form, same tier as に関する/に対する
      // によると/によれば stay split: に + よる + と/ば crosses the quotative と / conditional ば.

      // Place/Situation (場所・状況)
      particle("において", EPOS::ParticleCase),  // prevent に+おい(verb)+て split
      particle("における", EPOS::ParticleCase),  // prevent に+おける split
      particle("にて", EPOS::ParticleCase),

      // Capacity/Viewpoint (資格・観点)
      particle("として", EPOS::ParticleCase),  // prevent と+し(VERB)+て split
      particle("にとって", EPOS::ParticleCase),
      // にとっても removed — MeCab splits as にとって+も
      particle("に関して", EPOS::ParticleCase),
      particle("に関し", EPOS::ParticleCase),
      particle("に関する", EPOS::ParticleCase),
      particle("に際して", EPOS::ParticleCase),
      particle("に際し", EPOS::ParticleCase),
      particle("に対して", EPOS::ParticleCase),
      particle("に対し", EPOS::ParticleCase),
      particle("に対する", EPOS::ParticleCase),

      // Duration/Scope (範囲・期間)
      particle("にわたって", EPOS::ParticleCase),
      particle("にわたり", EPOS::ParticleCase),
      particle("にわたる", EPOS::ParticleCase),
      particle("にあたって", EPOS::ParticleCase),
      particle("にあたり", EPOS::ParticleCase),
      particle("に当たって", EPOS::ParticleCase),
      particle("に当たり", EPOS::ParticleCase),
      adv("わりに", "わりに"),  // concessive degree/expectation marker

      // Topic/Means (話題・手段)
      particle("をめぐって", EPOS::ParticleCase),
      // Note: をめぐり / をもって removed - grammatically を + 連用形 (めぐり, もっ+て);
      // the continuative keeps its own morpheme boundary
      particle("を通じて", EPOS::ParticleCase),
      particle("を通して", EPOS::ParticleCase),
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
