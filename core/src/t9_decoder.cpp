#include "t9_decoder.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "syllable_table.h"

namespace ninegrid {

namespace {

struct State {
  double score = 0.0;
  std::vector<std::string> syllables;
};

double path_bonus(const T9Path& path, const Lexicon* lexicon) {
  double bonus = 0.0;
  if (lexicon != nullptr) {
    const double freq = lexicon->max_freq(path.pinyin);
    if (freq > 0.0) {
      bonus += 2.0 + 0.5 * std::log10(freq + 1.0);
    }
  }
  if (path.syllables.size() == 1 && path.syllables[0].size() > 1) {
    bonus += 1.2;
  }
  return bonus;
}

}  // namespace

T9Result T9Decoder::decode(const std::string& digits, const Lexicon* lexicon) const {
  T9Result result;
  if (digits.empty()) {
    return result;
  }

  const SyllableTable& table = SyllableTable::instance();
  const std::size_t n = digits.size();
  std::vector<std::vector<State>> best(n + 1);
  best[0].push_back(State{0.0, {}});

  for (std::size_t i = 1; i <= n; ++i) {
    std::vector<State> bucket;
    for (const SyllableInfo& syllable : table.all()) {
      const std::size_t len = syllable.text.size();
      if (len > i) {
        continue;
      }
      if (syllable.digits != digits.substr(i - len, len)) {
        continue;
      }
      const std::vector<State>& prevs = best[i - len];
      if (prevs.empty()) {
        continue;
      }
      const double step = std::log(syllable.weight) + (len > 2 ? 0.3 * (len - 2) : 0.0);
      for (const State& prev : prevs) {
        State next;
        next.score = prev.score + step;
        next.syllables = prev.syllables;
        next.syllables.push_back(syllable.text);
        bucket.push_back(std::move(next));
      }
    }
    std::sort(bucket.begin(), bucket.end(),
              [](const State& a, const State& b) { return a.score > b.score; });
    if (bucket.size() > 8) {
      bucket.resize(8);
    }
    best[i] = std::move(bucket);
  }

  std::size_t covered = 0;
  for (std::size_t i = n; i >= 1; --i) {
    if (!best[i].empty()) {
      covered = i;
      break;
    }
  }
  if (covered == 0) {
    result.remainder = digits;
    return result;
  }

  result.covered = covered;
  result.remainder = digits.substr(covered);

  std::unordered_set<std::string> seen;
  std::vector<T9Path> paths;
  for (const State& state : best[covered]) {
    T9Path path;
    path.syllables = state.syllables;
    path.pinyin = Lexicon::join(path.syllables, 0, path.syllables.size());
    path.score = state.score + path_bonus(path, lexicon);
    if (!seen.insert(path.pinyin).second) {
      continue;
    }
    paths.push_back(std::move(path));
  }

  std::sort(paths.begin(), paths.end(),
            [](const T9Path& a, const T9Path& b) { return a.score > b.score; });
  if (paths.size() > 5) {
    paths.resize(5);
  }
  result.paths = std::move(paths);
  return result;
}

}  // namespace ninegrid
