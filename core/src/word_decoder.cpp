#include "word_decoder.h"

#include <cmath>
#include <limits>

namespace ninegrid {

WordSegment WordDecoder::best(const T9Path& path, const Lexicon& lexicon, const UserModel& user,
                              const std::string& prev_word, int64_t now) const {
  const std::size_t n = path.syllables.size();
  if (n == 0) {
    return {};
  }

  struct Node {
    bool valid = false;
    double score = -std::numeric_limits<double>::infinity();
    std::vector<const LexEntry*> words;
  };

  const double total = std::max(1.0, lexicon.total_freq());
  std::vector<Node> nodes(n + 1);
  nodes[0].valid = true;
  nodes[0].score = 0.0;

  for (std::size_t i = 1; i <= n; ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (!nodes[j].valid) {
        continue;
      }
      const std::string key = Lexicon::join(path.syllables, j, i);
      const std::vector<LexEntry>* entries = lexicon.lookup(key);
      if (entries == nullptr) {
        continue;
      }
      const std::string& context =
          j == 0 ? prev_word : nodes[j].words.back()->word;
      for (const LexEntry& entry : *entries) {
        const double base = std::log(entry.freq / total);
        const double length_bonus = 0.15 * static_cast<double>((i - j) > 0 ? (i - j - 1) : 0);
        const double boost = user.word_boost(entry.word, now) +
                             1.2 * user.bigram_boost(context, entry.word, now);
        const double score = nodes[j].score + base + length_bonus + boost;
        if (!nodes[i].valid || score > nodes[i].score) {
          nodes[i].valid = true;
          nodes[i].score = score;
          nodes[i].words = nodes[j].words;
          nodes[i].words.push_back(&entry);
        }
      }
    }
  }

  WordSegment result;
  if (!nodes[n].valid) {
    return result;
  }
  result.valid = true;
  result.score = nodes[n].score;
  result.words = nodes[n].words;
  return result;
}

}  // namespace ninegrid
