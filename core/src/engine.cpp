#include "ninegrid/engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "lexicon.h"
#include "t9_decoder.h"
#include "user_model.h"
#include "utf8.h"
#include "word_decoder.h"

namespace ninegrid {
namespace {

thread_local std::string g_error;

void set_error(const std::string& message) { g_error = message; }

int64_t now_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

struct Candidate {
  std::string word;
  std::string pinyin;
  double score = 0.0;
  int flags = 0;
};

class Engine {
 public:
  bool init(const std::string& dict_path, const std::string& user_path) {
    std::string error;
    if (!lexicon_.load(dict_path, &error)) {
      set_error(error);
      return false;
    }
    user_path_ = user_path;
    if (!user_path_.empty()) {
      if (!user_.load(user_path_, &error)) {
        set_error(error);
        return false;
      }
    }
    return true;
  }

  ~Engine() {
    if (!user_path_.empty() && user_.dirty()) {
      std::string error;
      user_.save(user_path_, &error);
    }
  }

  int press_digit(int digit) {
    if (digit < 2 || digit > 9) {
      set_error("digit must be 2..9");
      return -1;
    }
    if (digits_.size() + 1 >= NG_DIGITS_MAX) {
      set_error("digit buffer full");
      return -1;
    }
    digits_.push_back(static_cast<char>('0' + digit));
    path_index_ = 0;
    update();
    return 0;
  }

  int backspace() {
    if (digits_.empty()) {
      return 0;
    }
    digits_.pop_back();
    path_index_ = 0;
    update();
    return 0;
  }

  int cycle_path() {
    if (result_.paths.size() < 2) {
      return 0;
    }
    path_index_ = (path_index_ + 1) % static_cast<int>(result_.paths.size());
    build_candidates();
    update_composition();
    return 0;
  }

  int reset() {
    digits_.clear();
    prev_word_.clear();
    path_index_ = 0;
    result_ = T9Result{};
    candidates_.clear();
    composition_.clear();
    return 0;
  }

  int select(int index, std::string* out) {
    if (index < 0 || index >= static_cast<int>(candidates_.size())) {
      set_error("candidate index out of range");
      return -1;
    }
    const std::string word = candidates_[index].word;
    if (learning_ && !word.empty()) {
      user_.record(word, prev_word_, now_seconds());
    }
    prev_word_ = word;
    digits_.clear();
    path_index_ = 0;
    update();
    if (out != nullptr) {
      *out = word;
    }
    return 0;
  }

  void snapshot(NgSnapshot* out) const {
    std::memset(out, 0, sizeof(*out));
    copy_utf8(out->digits, sizeof(out->digits), digits_);
    copy_utf8(out->composition, sizeof(out->composition), composition_);
    out->path_count = static_cast<int32_t>(result_.paths.size());
    out->path_index = static_cast<int32_t>(path_index_);
    const int count = std::min<int>(static_cast<int>(candidates_.size()), NG_MAX_CANDIDATES);
    out->candidate_count = count;
    for (int i = 0; i < count; ++i) {
      copy_utf8(out->candidates[i].word, NG_WORD_MAX, candidates_[i].word);
      copy_utf8(out->candidates[i].pinyin, NG_PINYIN_MAX, candidates_[i].pinyin);
      out->candidates[i].score = static_cast<float>(candidates_[i].score);
      out->candidates[i].flags = candidates_[i].flags;
    }
  }

  void set_learning(bool enabled) { learning_ = enabled; }

  void clear_user() {
    user_.clear();
    if (!user_path_.empty()) {
      std::string error;
      user_.save(user_path_, &error);
    }
    build_candidates();
  }

  int save_user() {
    if (user_path_.empty()) {
      return 0;
    }
    std::string error;
    if (!user_.save(user_path_, &error)) {
      set_error(error);
      return -1;
    }
    return 0;
  }

  std::size_t candidate_count() const { return candidates_.size(); }

 private:
  void update() {
    result_ = decoder_.decode(digits_, &lexicon_);
    if (path_index_ >= static_cast<int>(result_.paths.size())) {
      path_index_ = 0;
    }
    build_candidates();
    update_composition();
  }

  void update_composition() {
    composition_.clear();
    if (digits_.empty()) {
      return;
    }
    if (result_.paths.empty()) {
      composition_ = digits_;
      if (digits_.size() > 1) {
        composition_ += "\xE2\x80\xA6";  // …
      }
      return;
    }
    const T9Path& path = result_.paths[path_index_];
    if (path.syllables.size() == 1 && path.syllables[0].size() == 1 && digits_.size() == 1) {
      composition_ = digits_;
      return;
    }
    composition_ = path.pinyin;
    if (result_.covered < digits_.size()) {
      composition_ += "\xE2\x80\xA6";
    }
  }

  bool learned(const std::string& word) const {
    return user_.effective_count(word, now_seconds()) > 0.0;
  }

  double rank(const LexEntry& entry, bool full_coverage, int* flags) const {
    const int64_t now = now_seconds();
    const double total = std::max(1.0, lexicon_.total_freq());
    double score = std::log(entry.freq / total);
    score += 1.0 * user_.word_boost(entry.word, now);
    score += 1.2 * user_.bigram_boost(prev_word_, entry.word, now);
    score += 0.3 * user_.recency(entry.word, now);
    if (full_coverage) {
      score += 0.8;
    }
    const std::size_t chars = utf8_length(entry.word);
    if (chars <= 1) {
      score -= 0.10;
      *flags |= 8;
    }
    if (learned(entry.word)) {
      *flags |= 4;
    }
    *flags |= 2;
    return score;
  }

  void add_candidate(const std::string& word, const std::string& pinyin, double score,
                     int flags) {
    if (word.empty()) {
      return;
    }
    for (Candidate& existing : candidates_) {
      if (existing.word == word) {
        existing.flags |= flags;
        if (score > existing.score) {
          existing.score = score;
          existing.pinyin = pinyin;
        }
        return;
      }
    }
    candidates_.push_back(Candidate{word, pinyin, score, flags});
  }

  void build_candidates() {
    candidates_.clear();
    if (digits_.empty() || result_.paths.empty()) {
      return;
    }

    const T9Path& path = result_.paths[path_index_];
    const int64_t now = now_seconds();
    const WordSegment segment = word_decoder_.best(path, lexicon_, user_, prev_word_, now);

    if (segment.valid && !segment.words.empty()) {
      std::string sentence;
      for (const LexEntry* entry : segment.words) {
        sentence += entry->word;
      }
      int flags = 1;
      for (const LexEntry* entry : segment.words) {
        if (learned(entry->word)) {
          flags |= 4;
          break;
        }
      }
      add_candidate(sentence, path.pinyin, segment.score + 1.0, flags);
      for (const LexEntry* entry : segment.words) {
        int word_flags = 2;
        const double score = rank(*entry, true, &word_flags);
        add_candidate(entry->word, entry->pinyin, score, word_flags);
      }
    }

    if (const std::vector<LexEntry>* full = lexicon_.lookup(path.pinyin)) {
      for (const LexEntry& entry : *full) {
        int flags = 0;
        add_candidate(entry.word, entry.pinyin, rank(entry, true, &flags), flags);
      }
    }

    for (const std::string& syllable : path.syllables) {
      if (const std::vector<LexEntry>* list = lexicon_.lookup(syllable)) {
        for (const LexEntry& entry : *list) {
          int flags = 0;
          add_candidate(entry.word, entry.pinyin, rank(entry, false, &flags), flags);
        }
      }
    }

    std::sort(candidates_.begin(), candidates_.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    if (candidates_.size() > NG_MAX_CANDIDATES) {
      candidates_.resize(NG_MAX_CANDIDATES);
    }
  }

  Lexicon lexicon_;
  UserModel user_;
  T9Decoder decoder_;
  WordDecoder word_decoder_;
  std::string digits_;
  std::string composition_;
  std::string user_path_;
  std::string prev_word_;
  T9Result result_;
  int path_index_ = 0;
  bool learning_ = true;
  std::vector<Candidate> candidates_;
};

}  // namespace
}  // namespace ninegrid

extern "C" {

NgEngine* ng_create(const char* dict_path, const char* user_path) {
  if (dict_path == nullptr || *dict_path == '\0') {
    ninegrid::set_error("dict_path is required");
    return nullptr;
  }
  try {
    auto* engine = new ninegrid::Engine();
    const std::string user = user_path == nullptr ? std::string() : std::string(user_path);
    if (!engine->init(dict_path, user)) {
      delete engine;
      return nullptr;
    }
    ninegrid::set_error("");
    return reinterpret_cast<NgEngine*>(engine);
  } catch (const std::exception& ex) {
    ninegrid::set_error(std::string("create failed: ") + ex.what());
    return nullptr;
  } catch (...) {
    ninegrid::set_error("create failed: unknown error");
    return nullptr;
  }
}

void ng_destroy(NgEngine* engine) {
  delete reinterpret_cast<ninegrid::Engine*>(engine);
}

int32_t ng_press_digit(NgEngine* engine, int32_t digit) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  return reinterpret_cast<ninegrid::Engine*>(engine)->press_digit(digit);
}

int32_t ng_backspace(NgEngine* engine) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  return reinterpret_cast<ninegrid::Engine*>(engine)->backspace();
}

int32_t ng_cycle_path(NgEngine* engine) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  return reinterpret_cast<ninegrid::Engine*>(engine)->cycle_path();
}

int32_t ng_reset(NgEngine* engine) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  return reinterpret_cast<ninegrid::Engine*>(engine)->reset();
}

int32_t ng_select(NgEngine* engine, int32_t index, char* out, int32_t out_cap) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  std::string word;
  const int status = reinterpret_cast<ninegrid::Engine*>(engine)->select(index, &word);
  if (status != 0) {
    return status;
  }
  if (out != nullptr && out_cap > 0) {
    ninegrid::copy_utf8(out, static_cast<std::size_t>(out_cap), word);
  }
  return 0;
}

int32_t ng_snapshot(NgEngine* engine, NgSnapshot* out) {
  if (engine == nullptr || out == nullptr) {
    ninegrid::set_error("engine or snapshot is null");
    return -1;
  }
  reinterpret_cast<ninegrid::Engine*>(engine)->snapshot(out);
  return 0;
}

int32_t ng_set_learning(NgEngine* engine, int32_t enabled) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  reinterpret_cast<ninegrid::Engine*>(engine)->set_learning(enabled != 0);
  return 0;
}

int32_t ng_clear_user(NgEngine* engine) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  reinterpret_cast<ninegrid::Engine*>(engine)->clear_user();
  return 0;
}

int32_t ng_save_user(NgEngine* engine) {
  if (engine == nullptr) {
    ninegrid::set_error("engine is null");
    return -1;
  }
  return reinterpret_cast<ninegrid::Engine*>(engine)->save_user();
}

const char* ng_last_error(void) { return ninegrid::g_error.c_str(); }

}  // extern "C"
