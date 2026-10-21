#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#include "lexicon.h"
#include "ninegrid/engine.h"
#include "syllable_table.h"
#include "t9_decoder.h"
#include "user_model.h"

namespace {

int g_failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #condition    \
                << "\n";                                                       \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

std::string temp_user_path() {
  return "/tmp/ninegrid_test_user_" + std::to_string(getpid()) + ".tsv";
}

void press(NgEngine* engine, const std::string& digits) {
  for (char ch : digits) {
    if (ng_press_digit(engine, ch - '0') != 0) {
      std::cerr << "press failed: " << ng_last_error() << "\n";
    }
  }
}

int find_candidate(const NgSnapshot& snapshot, const std::string& word) {
  for (int i = 0; i < snapshot.candidate_count; ++i) {
    if (word == snapshot.candidates[i].word) {
      return i;
    }
  }
  return -1;
}

void test_syllables() {
  const ninegrid::SyllableTable& table = ninegrid::SyllableTable::instance();
  CHECK(table.encode("ni") == "64");
  CHECK(table.encode("hao") == "426");
  CHECK(table.encode("xian") == "9426");
  CHECK(table.encode("ni'hao").empty());
  CHECK(table.is_valid("ni"));
  CHECK(table.is_valid("xian"));
  CHECK(!table.is_valid("xx"));
  CHECK(table.all().size() > 300);
}

void test_decode(const std::string& dict_path) {
  ninegrid::Lexicon lexicon;
  std::string error;
  CHECK(lexicon.load(dict_path, &error));
  CHECK(lexicon.entry_count() >= 90);

  ninegrid::T9Decoder decoder;
  const ninegrid::T9Result result = decoder.decode("64426", &lexicon);
  CHECK(!result.paths.empty());
  if (!result.paths.empty()) {
    CHECK(result.paths[0].pinyin == "ni'hao");
    CHECK(result.covered == 5);
    CHECK(result.remainder.empty());
  }

  const ninegrid::T9Result split = decoder.decode("9426", &lexicon);
  bool has_xian = false;
  bool has_xi_an = false;
  for (const auto& path : split.paths) {
    if (path.pinyin == "xian") has_xian = true;
    if (path.pinyin == "xi'an") has_xi_an = true;
  }
  CHECK(has_xian);
  CHECK(has_xi_an);

  const ninegrid::T9Result partial = decoder.decode("644", &lexicon);
  CHECK(partial.covered == 2);
  CHECK(partial.remainder == "4");
  CHECK(!partial.paths.empty());
}

void test_engine_basic(const std::string& dict_path) {
  const std::string user_path = temp_user_path();
  std::remove(user_path.c_str());

  NgEngine* engine = ng_create(dict_path.c_str(), user_path.c_str());
  CHECK(engine != nullptr);
  if (engine == nullptr) {
    std::cerr << "create failed: " << ng_last_error() << "\n";
    return;
  }

  press(engine, "64426");
  NgSnapshot snapshot;
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(std::string(snapshot.composition) == "ni'hao");
  CHECK(snapshot.candidate_count > 0);
  if (snapshot.candidate_count > 0) {
    CHECK(std::string(snapshot.candidates[0].word) == "你好");
  }

  char output[NG_WORD_MAX] = {0};
  CHECK(ng_select(engine, 0, output, sizeof(output)) == 0);
  CHECK(std::string(output) == "你好");
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(snapshot.digits[0] == '\0');

  ng_destroy(engine);
  std::remove(user_path.c_str());
}

void test_cycle_path(const std::string& dict_path) {
  const std::string user_path = temp_user_path();
  std::remove(user_path.c_str());

  NgEngine* engine = ng_create(dict_path.c_str(), user_path.c_str());
  CHECK(engine != nullptr);
  if (engine == nullptr) {
    return;
  }

  press(engine, "9426");
  NgSnapshot snapshot;
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(snapshot.path_count >= 2);
  CHECK(std::string(snapshot.composition) == "xian");
  CHECK(snapshot.candidate_count > 0);
  CHECK(std::string(snapshot.candidates[0].word) == "先");

  CHECK(ng_cycle_path(engine) == 0);
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(std::string(snapshot.composition) == "xi'an");
  CHECK(snapshot.candidate_count > 0);
  CHECK(std::string(snapshot.candidates[0].word) == "西安");

  ng_destroy(engine);
  std::remove(user_path.c_str());
}

void test_learning(const std::string& dict_path) {
  const std::string user_path = temp_user_path();
  std::remove(user_path.c_str());

  NgEngine* engine = ng_create(dict_path.c_str(), user_path.c_str());
  CHECK(engine != nullptr);
  if (engine == nullptr) {
    return;
  }

  for (int i = 0; i < 40; ++i) {
    press(engine, "64");
    NgSnapshot snapshot;
    CHECK(ng_snapshot(engine, &snapshot) == 0);
    const int index = find_candidate(snapshot, "尼");
    CHECK(index >= 0);
    if (index < 0) {
      break;
    }
    char output[NG_WORD_MAX] = {0};
    CHECK(ng_select(engine, index, output, sizeof(output)) == 0);
  }

  press(engine, "64");
  NgSnapshot snapshot;
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(snapshot.candidate_count > 0);
  if (snapshot.candidate_count > 0) {
    CHECK(std::string(snapshot.candidates[0].word) == "尼");
  }

  CHECK(ng_clear_user(engine) == 0);
  press(engine, "64");
  CHECK(ng_snapshot(engine, &snapshot) == 0);
  CHECK(snapshot.candidate_count > 0);
  if (snapshot.candidate_count > 0) {
    CHECK(std::string(snapshot.candidates[0].word) == "你");
  }

  ng_destroy(engine);
  std::remove(user_path.c_str());
}

void test_persistence() {
  const std::string path = temp_user_path();
  std::remove(path.c_str());

  const int64_t now = 1700000000;
  ninegrid::UserModel model;
  model.record("你好", "", now);
  model.record("你好", "", now + 10);
  CHECK(model.word_count() == 1);
  CHECK(model.dirty());
  std::string error;
  CHECK(model.save(path, &error));

  ninegrid::UserModel loaded;
  CHECK(loaded.load(path, &error));
  CHECK(loaded.word_count() == 1);
  CHECK(loaded.effective_count("你好", now + 10) > 1.9);

  loaded.clear();
  CHECK(loaded.save(path, &error));
  std::remove(path.c_str());
}

void test_misc(const std::string& dict_path) {
  const std::string user_path = temp_user_path();
  std::remove(user_path.c_str());

  NgEngine* engine = ng_create(dict_path.c_str(), user_path.c_str());
  CHECK(engine != nullptr);
  if (engine != nullptr) {
    press(engine, "6");
    NgSnapshot snapshot;
    CHECK(ng_snapshot(engine, &snapshot) == 0);
    CHECK(std::string(snapshot.composition) == "6");
    CHECK(ng_press_digit(engine, 1) == -1);
    CHECK(ng_backspace(engine) == 0);
    CHECK(ng_reset(engine) == 0);
    ng_destroy(engine);
  }
  std::remove(user_path.c_str());

  CHECK(ng_create("/no/such/dict.tsv", "") == nullptr);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dict_path = argc > 1 ? argv[1] : "data/sample_dict.tsv";
  std::cout << "ninegrid core tests · dict=" << dict_path << "\n";

  test_syllables();
  test_decode(dict_path);
  test_engine_basic(dict_path);
  test_cycle_path(dict_path);
  test_learning(dict_path);
  test_persistence();
  test_misc(dict_path);

  if (g_failures == 0) {
    std::cout << "ALL TESTS PASSED\n";
    return 0;
  }
  std::cerr << g_failures << " test(s) failed\n";
  return 1;
}
