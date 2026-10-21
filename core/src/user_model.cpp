#include "user_model.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ninegrid {

namespace {

std::vector<std::string> split_tabs(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;
  for (char ch : line) {
    if (ch == '\t') {
      fields.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  fields.push_back(current);
  return fields;
}

bool parse_double(const std::string& text, double* out) {
  try {
    std::size_t used = 0;
    const double value = std::stod(text, &used);
    if (used == 0) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool parse_i64(const std::string& text, int64_t* out) {
  try {
    std::size_t used = 0;
    const long long value = std::stoll(text, &used);
    if (used == 0) {
      return false;
    }
    *out = static_cast<int64_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

std::string UserModel::bigram_key(const std::string& prev_word, const std::string& word) {
  std::string key = prev_word;
  key.push_back('\x1f');
  key += word;
  return key;
}

double UserModel::effective_count(const std::string& word, int64_t now) const {
  const auto it = words_.find(word);
  if (it == words_.end()) {
    return 0.0;
  }
  const Stat& stat = it->second;
  if (stat.last <= 0 || now <= stat.last) {
    return stat.count;
  }
  const double age = static_cast<double>(now - stat.last);
  return stat.count * std::exp(-age / kDefaultTauSeconds);
}

double UserModel::word_boost(const std::string& word, int64_t now) const {
  return std::log1p(effective_count(word, now));
}

double UserModel::bigram_boost(const std::string& prev_word, const std::string& word,
                               int64_t now) const {
  if (prev_word.empty()) {
    return 0.0;
  }
  const auto it = bigrams_.find(bigram_key(prev_word, word));
  if (it == bigrams_.end()) {
    return 0.0;
  }
  const Stat& stat = it->second;
  double count = stat.count;
  if (stat.last > 0 && now > stat.last) {
    count *= std::exp(-static_cast<double>(now - stat.last) / kDefaultTauSeconds);
  }
  return std::log1p(count);
}

double UserModel::recency(const std::string& word, int64_t now) const {
  const auto it = words_.find(word);
  if (it == words_.end() || it->second.last <= 0) {
    return 0.0;
  }
  if (now <= it->second.last) {
    return 1.0;
  }
  return std::exp(-static_cast<double>(now - it->second.last) / kRecencyTauSeconds);
}

int64_t UserModel::last_used(const std::string& word) const {
  const auto it = words_.find(word);
  return it == words_.end() ? 0 : it->second.last;
}

void UserModel::record(const std::string& word, const std::string& prev_word, int64_t now) {
  if (word.empty()) {
    return;
  }
  Stat& stat = words_[word];
  stat.count = effective_count(word, now) + 1.0;
  stat.last = now;

  if (!prev_word.empty()) {
    Stat& bigram = bigrams_[bigram_key(prev_word, word)];
    double count = bigram.count;
    if (bigram.last > 0 && now > bigram.last) {
      count *= std::exp(-static_cast<double>(now - bigram.last) / kDefaultTauSeconds);
    }
    bigram.count = count + 1.0;
    bigram.last = now;
  }
  dirty_ = true;
}

bool UserModel::load(const std::string& path, std::string* error) {
  (void)error;
  words_.clear();
  bigrams_.clear();
  dirty_ = false;

  std::ifstream input(path);
  if (!input) {
    // Missing user file is normal on first run.
    return true;
  }

  std::string line;
  std::size_t line_no = 0;
  while (std::getline(input, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<std::string> fields = split_tabs(line);
    if (fields.empty()) {
      continue;
    }

    if (fields[0] == "W" && fields.size() == 4) {
      double count = 0.0;
      int64_t last = 0;
      if (parse_double(fields[2], &count) && parse_i64(fields[3], &last)) {
        words_[fields[1]] = Stat{count, last};
      }
    } else if (fields[0] == "B" && fields.size() == 5) {
      double count = 0.0;
      int64_t last = 0;
      if (parse_double(fields[3], &count) && parse_i64(fields[4], &last)) {
        bigrams_[bigram_key(fields[1], fields[2])] = Stat{count, last};
      }
    }
  }
  return true;
}

bool UserModel::save(const std::string& path, std::string* error) {
  std::ofstream output(path, std::ios::trunc);
  if (!output) {
    if (error != nullptr) {
      *error = "cannot write user model: " + path;
    }
    return false;
  }

  output << "# ninegrid user model v1\n";
  output << std::setprecision(12);
  for (const auto& pair : words_) {
    output << "W\t" << pair.first << '\t' << pair.second.count << '\t' << pair.second.last
           << '\n';
  }
  for (const auto& pair : bigrams_) {
    const std::string& key = pair.first;
    const std::size_t sep = key.find('\x1f');
    if (sep == std::string::npos) {
      continue;
    }
    output << "B\t" << key.substr(0, sep) << '\t' << key.substr(sep + 1) << '\t'
           << pair.second.count << '\t' << pair.second.last << '\n';
  }
  output.flush();
  if (!output) {
    if (error != nullptr) {
      *error = "failed while writing user model: " + path;
    }
    return false;
  }
  dirty_ = false;
  return true;
}

void UserModel::clear() {
  words_.clear();
  bigrams_.clear();
  dirty_ = true;
}

}  // namespace ninegrid
