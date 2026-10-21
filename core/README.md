# 九格输入核心（C++17）

跨平台输入引擎：T9 解码、词图切分、候选排序、本地学习与持久化。不依赖 Android / HarmonyOS / iOS，可单独构建与测试。

## 当前状态（v0.1 原型）

已实现：

- 数字串 → 拼音音节路径（Viterbi + 音节权重 + 长音节/整词奖励）
- 音节路径 → 词图切分（词典 DP）
- 候选生成与排序（基础词频 + 个人词频 + 上下文 + 最近使用）
- 音节/分词切换（`ng_cycle_path`）
- 本地用户模型：词频、bigram、时间衰减、保存/加载/清除
- C API（`include/ninegrid/engine.h`）
- 命令行交互 demo 与单元测试

暂未实现（下一步）：

- 模糊音、简拼、邻键纠错
- 二进制词库 `.nglex`（当前为 TSV，样例 95 词）
- 大规模词库性能优化（当前词典为哈希表 + 线性候选合并）
- 用户词导入/导出 JSON、屏蔽词
- NAPI / JNI / C ABI 桥接（在平台工程里做）

## 构建与测试

本机（macOS，无 CMake）用 Makefile：

```bash
cd core
make -j4
make test
```

有 CMake 的环境：

```bash
cd core
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## 命令行 demo

```bash
cd core
./build/ninegrid_cli --dict data/sample_dict.tsv --user /tmp/ninegrid_user.tsv
```

交互：

```
64426        # 追加数字
s1           # 选第 1 个候选
c            # 切换分词（如 9426: xian <-> xi'an）
b            # 删除一个数字
r            # 重置
q            # 退出
```

示例：

```
数字: 64426
拼音: ni'hao
  1. 你好 [句词]
  2. 你 [词字]
  3. 好 [词字]
```

## 词库格式

TSV，UTF-8，一行一词：

```
你好<TAB>ni'hao<TAB>100000
西安<TAB>xi'an<TAB>20000
先<TAB>xian<TAB>50000
```

- 第二列必须按音节用 `'` 分隔；单个音节直接写（`ni`）
- 第三列是基础词频，缺省 1
- 多音词按不同拼音写多行
- 注释以 `#` 开头

样例：[data/sample_dict.tsv](data/sample_dict.tsv)。

## C API

```c
NgEngine* ng_create(const char* dict_path, const char* user_path);
int32_t   ng_press_digit(NgEngine*, int32_t digit); /* 2..9 */
int32_t   ng_backspace(NgEngine*);
int32_t   ng_cycle_path(NgEngine*);
int32_t   ng_select(NgEngine*, int32_t index, char* out, int32_t cap);
int32_t   ng_snapshot(NgEngine*, NgSnapshot* out);
void      ng_destroy(NgEngine*);
```

`NgSnapshot` 包含组合串、拼音、分词路径数与当前索引、最多 12 个候选（词、拼音、分数、flags：1 句 / 2 词 / 4 学 / 8 单字）。

## 目录

```
include/ninegrid/engine.h   C API
src/syllable_table.*        音节表与数字映射
src/t9_decoder.*            数字 → 拼音路径
src/lexicon.*               TSV 词库
src/word_decoder.*          音节 → 词图切分
src/user_model.*            个人词频 / bigram / 衰减 / 持久化
src/engine.cpp              状态机 + 候选排序 + C API
cli/main.cpp                命令行 demo
tests/test_core.cpp         单元测试
```

## 下一步

1. 把样例词库替换为 38–42 万条的 A 档词库，编译为 `.nglex` 并做 mmap。
2. 在鸿蒙工程里通过 NAPI 暴露同一 C API；ArkTS 只渲染 `NgSnapshot`。
3. 加模糊音/简拼/纠错，再用 500 句评测集调排序权重。
