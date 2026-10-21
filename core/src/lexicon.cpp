#include "lexicon.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "syllable_table.h"

namespace ninegrid {

namespace {

std::string trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(begin, end - begin);
}

std::vector<std::string> split(const std::string& value, char sep) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : value) {
    if (ch == sep) {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  parts.push_back(current);
  return parts;
}

// Split a concatenated pinyin (no separator) by longest valid syllable.
std::vector<std::string> greedy_split(const std::string& text, bool* ok) {
  const SyllableTable& table = SyllableTable::instance();
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t best = 0;
    for (std::size_t len = std::min<std::size_t>(6, text.size() - i); len >= 1; --len) {
      const std::string piece = text.substr(i, len);
      if (table.is_valid(piece)) {
        best = len;
        break;
      }
    }
    if (best == 0) {
      *ok = false;
      return {};
    }
    out.push_back(text.substr(i, best));
    i += best;
  }
  *ok = true;
  return out;
}

}  // namespace

std::string Lexicon::canonical_pinyin(const std::string& raw, bool* ok) {
  *ok = false;
  std::string text = trim(raw);
  if (text.empty()) {
    return {};
  }
  for (char& ch : text) {
    if (ch == ' ' || ch == '\t' || ch == '-') {
      ch = '\'';
    }
  }
  std::vector<std::string> parts = split(text, '\'');
  std::vector<std::string> syllables;
  for (const std::string& part : parts) {
    if (part.empty()) {
      continue;
    }
    syllables.push_back(part);
  }
  if (syllables.empty()) {
    return {};
  }

  const SyllableTable& table = SyllableTable::instance();
  if (syllables.size() == 1 && !table.is_valid(syllables[0])) {
    bool split_ok = false;
    std::vector<std::string> expanded = greedy_split(syllables[0], &split_ok);
    if (!split_ok) {
      return {};
    }
    syllables = expanded;
  }

  for (const std::string& syllable : syllables) {
    if (!table.is_valid(syllable)) {
      return {};
    }
  }
  *ok = true;
  return join(syllables, 0, syllables.size());
}

std::string Lexicon::join(const std::vector<std::string>& syllables, std::size_t begin,
                          std::size_t end) {
  std::string out;
  for (std::size_t i = begin; i < end; ++i) {
    if (!out.empty()) {
      out.push_back('\'');
    }
    out += syllables[i];
  }
  return out;
}

bool Lexicon::load(const std::string& path, std::string* error) {
  entries_.clear();
  entry_count_ = 0;
  total_freq_ = 0.0;

  std::ifstream input(path);
  if (!input) {
    if (error != nullptr) {
      *error = "cannot open dictionary: " + path;
    }
    return false;
  }

  std::string line;
  std::size_t line_no = 0;
  std::size_t skipped = 0;
  while (std::getline(input, line)) {
    ++line_no;
    if (line_no == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
      line.erase(0, 3);
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const std::vector<std::string> fields = split(line, '\t');
    if (fields.size() < 2) {
      ++skipped;
      continue;
    }

    const std::string word = trim(fields[0]);
    if (word.empty()) {
      ++skipped;
      continue;
    }

    bool ok = false;
    const std::string pinyin = canonical_pinyin(fields[1], &ok);
    if (!ok || pinyin.empty()) {
      ++skipped;
      continue;
    }

    double freq = 1.0;
    if (fields.size() >= 3) {
      try {
        freq = std::stod(trim(fields[2]));
      } catch (...) {
        freq = 1.0;
      }
    }
    if (freq <= 0.0) {
      freq = 1.0;
    }

    entries_[pinyin].push_back(LexEntry{word, pinyin, freq});
    ++entry_count_;
    total_freq_ += freq;
  }

  for (auto& pair : entries_) {
    auto& list = pair.second;
    std::sort(list.begin(), list.end(),
              [](const LexEntry& a, const LexEntry& b) { return a.freq > b.freq; });
    if (list.size() > 64) {
      list.resize(64);
    }
  }

  if (entry_count_ == 0) {
    if (error != nullptr) {
      *error = "dictionary is empty: " + path;
    }
    return false;
  }
  (void)skipped;
  return true;
}

const std::vector<LexEntry>* Lexicon::lookup(const std::string& pinyin) const {
  const auto it = entries_.find(pinyin);
  return it == entries_.end() ? nullptr : &it->second;
}

bool Lexicon::contains(const std::string& pinyin) const {
  return entries_.find(pinyin) != entries_.end();
}

double Lexicon::max_freq(const std::string& pinyin) const {
  const std::vector<LexEntry>* list = lookup(pinyin);
  if (list == nullptr || list->empty()) {
    return 0.0;
  }
  return list->front().freq;
}

}  // namespace ninegrid
