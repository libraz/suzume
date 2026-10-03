#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getInterjectionEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Common interjections (exclamations)
      intj("えっ"),    // Surprise
      intj("ええ"),    // Affirmation/Surprise
      intj("あっ"),    // Realization
      intj("ああ"),    // Agreement/Sigh
      intj("おお"),    // Amazement
      intj("おや"),    // Mild surprise or notice
      intj("うわ"),    // Surprise
      intj("うわっ"),  // Surprise (emphatic)
      intj("いや"),    // Disagreement/reluctance
      intj("いやー"),  // Prolonged conversational variant
      intj("わあ"),    // Amazement
      intj("へえ"),    // Interest
      intj("ふーん"),  // Understanding/Disinterest
      intj("ふうん"),  // Understanding
      // Note: ほう removed - formal noun usage (ほうがいい) is more common
      intj("おい"),    // Calling attention
      intj("おーい"),  // Calling from afar
      intj("あら"),    // Surprise (also the irrealis of ある; context decides)
      intj("あれ"),    // Confusion
      intj("あれっ"),  // Confusion (emphatic)
      intj("何だ"),    // Disappointment/Realization
      intj("まあ"),    // Surprise/Moderation
      intj("さあ"),    // Prompting/Urging
      intj("ねえ"),    // Attention-getting (also particle, but standalone usage)
      // Responses
      intj("はい"),    // Yes
      intj("いいえ"),  // No
      intj("うん"),    // Casual yes
      intj("ううん"),  // Casual no
      // Hesitation/Filler
      intj("えーと"),  // Hesitation
      intj("えっと"),  // Hesitation
      intj("ええと"),  // Hesitation
      intj("あの"),    // Hesitation (also determiner)
      intj("その"),    // Hesitation (rare, also determiner)
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
