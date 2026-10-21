#ifndef NINEGRID_WORD_DECODER_H
#define NINEGRID_WORD_DECODER_H

#include <cstdint>
#include <string>
#include <vector>

#include "lexicon.h"
#include "t9_decoder.h"
#include "user_model.h"

namespace ninegrid {

struct WordSegment {
  bool valid = false;
  double score = 0.0;
  std::vector<const LexEntry*> words;
};

class WordDecoder {
 public:
  WordSegment best(const T9Path& path, const Lexicon& lexicon, const UserModel& user,
                   const std::string& prev_word, int64_t now) const;
};

}  // namespace ninegrid

#endif  // NINEGRID_WORD_DECODER_H
