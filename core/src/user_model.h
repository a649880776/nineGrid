#ifndef NINEGRID_USER_MODEL_H
#define NINEGRID_USER_MODEL_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ninegrid {

class UserModel {
 public:
  static constexpr double kDefaultTauSeconds = 30.0 * 24.0 * 60.0 * 60.0;
  static constexpr double kRecencyTauSeconds = 7.0 * 24.0 * 60.0 * 60.0;

  void record(const std::string& word, const std::string& prev_word, int64_t now);

  double effective_count(const std::string& word, int64_t now) const;
  double word_boost(const std::string& word, int64_t now) const;
  double bigram_boost(const std::string& prev_word, const std::string& word, int64_t now) const;
  double recency(const std::string& word, int64_t now) const;
  int64_t last_used(const std::string& word) const;

  bool load(const std::string& path, std::string* error);
  bool save(const std::string& path, std::string* error);
  void clear();

  std::size_t word_count() const { return words_.size(); }
  std::size_t bigram_count() const { return bigrams_.size(); }
  bool dirty() const { return dirty_; }

  static std::string bigram_key(const std::string& prev_word, const std::string& word);

 private:
  struct Stat {
    double count = 0.0;
    int64_t last = 0;
  };

  std::unordered_map<std::string, Stat> words_;
  std::unordered_map<std::string, Stat> bigrams_;
  bool dirty_ = false;
};

}  // namespace ninegrid

#endif  // NINEGRID_USER_MODEL_H
