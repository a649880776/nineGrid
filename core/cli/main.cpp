#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "ninegrid/engine.h"

namespace {

std::string flags_text(int flags) {
  std::string text;
  if (flags & 1) text += "句";
  if (flags & 2) text += "词";
  if (flags & 4) text += "学";
  if (flags & 8) text += "字";
  return text;
}

void print_snapshot(const NgSnapshot& snapshot) {
  std::cout << "\n";
  if (snapshot.digits[0] == '\0') {
    std::cout << "(空闲)\n";
  } else {
    std::cout << "数字: " << snapshot.digits << "\n";
    std::cout << "拼音: " << snapshot.composition;
    if (snapshot.path_count > 1) {
      std::cout << "   [" << (snapshot.path_index + 1) << "/" << snapshot.path_count
                << " 分词]";
    }
    std::cout << "\n";
  }
  for (int i = 0; i < snapshot.candidate_count; ++i) {
    const NgCandidate& candidate = snapshot.candidates[i];
    std::cout << "  " << (i + 1) << ". " << candidate.word;
    const std::string flags = flags_text(candidate.flags);
    if (!flags.empty()) {
      std::cout << " [" << flags << "]";
    }
    std::cout << "  (" << candidate.pinyin << ")\n";
  }
  std::cout << std::flush;
}

void usage(const char* argv0) {
  std::cout << "用法: " << argv0 << " [--dict 词库.tsv] [--user 用户模型.tsv]\n"
            << "交互:\n"
            << "  输入 2-9 数字串追加输入\n"
            << "  sN      选择第 N 个候选（从 1 开始）\n"
            << "  b       删除一个数字\n"
            << "  c       切换分词\n"
            << "  r       重置\n"
            << "  q       退出\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string dict_path = "data/sample_dict.tsv";
  std::string user_path = "ninegrid_user.tsv";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--dict" && i + 1 < argc) {
      dict_path = argv[++i];
    } else if (arg == "--user" && i + 1 < argc) {
      user_path = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      usage(argv[0]);
      return 0;
    } else {
      std::cerr << "unknown argument: " << arg << "\n";
      usage(argv[0]);
      return 2;
    }
  }

  NgEngine* engine = ng_create(dict_path.c_str(), user_path.c_str());
  if (engine == nullptr) {
    std::cerr << "初始化失败: " << ng_last_error() << "\n";
    return 1;
  }

  std::cout << "九格 core CLI · 词库 " << dict_path << "\n";
  std::cout << "输入 2-9 数字，输入 sN 选第 N 个候选，c 分词，b 删除，r 重置，q 退出。\n";

  NgSnapshot snapshot;
  ng_snapshot(engine, &snapshot);
  print_snapshot(snapshot);

  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }
    if (line.empty()) {
      continue;
    }
    if (line == "q" || line == "quit") {
      break;
    }
    if (line == "b") {
      ng_backspace(engine);
    } else if (line == "c") {
      ng_cycle_path(engine);
    } else if (line == "r") {
      ng_reset(engine);
    } else if (line[0] == 's' && line.size() > 1) {
      const int index = std::atoi(line.c_str() + 1) - 1;
      char output[NG_WORD_MAX] = {0};
      if (ng_select(engine, index, output, sizeof(output)) == 0) {
        std::cout << "上屏: " << output << "\n";
      } else {
        std::cout << "选择失败: " << ng_last_error() << "\n";
      }
    } else {
      bool all_digits = true;
      for (char ch : line) {
        if (ch < '2' || ch > '9') {
          all_digits = false;
          break;
        }
      }
      if (!all_digits) {
        std::cout << "未知输入。数字只接受 2-9。\n";
        continue;
      }
      for (char ch : line) {
        ng_press_digit(engine, ch - '0');
      }
    }
    ng_snapshot(engine, &snapshot);
    print_snapshot(snapshot);
  }

  ng_destroy(engine);
  return 0;
}
