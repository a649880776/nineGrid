#ifndef NINEGRID_T9_DECODER_H
#define NINEGRID_T9_DECODER_H

#include <cstddef>
#include <string>
#include <vector>

#include "lexicon.h"

namespace ninegrid {

struct T9Path {
  std::vector<std::string> syllables;
  std::string pinyin;
  double score = 0.0;
};

struct T9Result {
  std::vector<T9Path> paths;
  std::size_t covered = 0;
  std::string remainder;
};

class T9Decoder {
 public:
  T9Result decode(const std::string& digits, const Lexicon* lexicon) const;
};

}  // namespace ninegrid

#endif  // NINEGRID_T9_DECODER_H
