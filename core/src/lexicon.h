#ifndef NINEGRID_LEXICON_H
#define NINEGRID_LEXICON_H

#include <string>
#include <unordered_map>
#include <vector>

namespace ninegrid {

struct LexEntry {
  std::string word;
  std::string pinyin;  // canonical: syllables joined by '
  double freq;
};

class Lexicon {
 public:
  bool load(const std::string& path, std::string* error);

  const std::vector<LexEntry>* lookup(const std::string& pinyin) const;
  bool contains(const std::string& pinyin) const;
  double max_freq(const std::string& pinyin) const;
  double total_freq() const { return total_freq_; }

  std::size_t entry_count() const { return entry_count_; }
  std::size_t key_count() const { return entries_.size(); }

  static std::string canonical_pinyin(const std::string& raw, bool* ok);
  static std::string join(const std::vector<std::string>& syllables, std::size_t begin,
                          std::size_t end);

 private:
  std::unordered_map<std::string, std::vector<LexEntry>> entries_;
  std::size_t entry_count_ = 0;
  double total_freq_ = 0.0;
};

}  // namespace ninegrid

#endif  // NINEGRID_LEXICON_H
