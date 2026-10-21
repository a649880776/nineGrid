#ifndef NINEGRID_SYLLABLE_TABLE_H
#define NINEGRID_SYLLABLE_TABLE_H

#include <string>
#include <unordered_map>
#include <vector>

namespace ninegrid {

struct SyllableInfo {
  std::string text;
  std::string digits;
  double weight;
};

class SyllableTable {
 public:
  static const SyllableTable& instance();

  const std::vector<SyllableInfo>& all() const { return all_; }

  bool is_valid(const std::string& syllable) const;
  std::string encode(const std::string& text) const;  // empty if not encodable
  double weight(const std::string& syllable) const;

  // "2".."9" for a-z, 0 otherwise.
  static char digit_of(char ch);

 private:
  SyllableTable();

  std::vector<SyllableInfo> all_;
  std::unordered_map<std::string, std::size_t> index_;
  std::unordered_map<std::string, double> weights_;
};

}  // namespace ninegrid

#endif  // NINEGRID_SYLLABLE_TABLE_H
