# 九格 NineGrid · core

九格是一款面向中国用户的九宫格输入法。本仓库**只开放 C++17 输入核心**；`docs/`、`platforms/`、`site/` 属于本地材料，不在 GitHub 发布。

核心不依赖 Android / HarmonyOS / iOS，可单独构建、测试，后续通过 NAPI / JNI / C ABI 接入各平台宿主。

## 当前实现

- 数字串 → 拼音音节路径（Viterbi + 音节权重 + 整词奖励）
- 音节 → 词图切分（词典 DP）
- 候选排序：基础词频 + 个人词频 + 上下文 + 最近使用
- 分词路径切换（`9426`：`xian` ⇄ `xi'an`）
- 本地用户模型：词频、bigram、时间衰减、保存/加载/清除
- C API：[core/include/ninegrid/engine.h](core/include/ninegrid/engine.h)
- 命令行 demo + 单元测试

## 构建

macOS / Linux（Makefile）：

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

## 试打

```bash
cd core
./build/ninegrid_cli --dict data/sample_dict.tsv --user /tmp/ninegrid_user.tsv
```

```
64426        # 追加数字
s1           # 选第 1 个候选
c            # 切换分词
b            # 删除一个数字
r            # 重置
q            # 退出
```

`make test` 覆盖：音节编码、`64426 → 你好`、`9426` 双路径、学习后排序、持久化、错误输入。

## 目录

```
core/
├─ include/ninegrid/engine.h   C API
├─ src/                        T9 / 词库 / 词图 / 排序 / 学习
├─ cli/                        命令行 demo
├─ tests/                      单元测试
├─ data/sample_dict.tsv        95 词样例词库
├─ CMakeLists.txt
└─ Makefile
```

## 状态与下一步

当前是 v0.1 原型，样例词库仅 95 词；下一步是替换为可再分发的 38–42 万条词库、编译 `.nglex`，并在鸿蒙工程中通过 NAPI 接入。

## 许可证

Apache-2.0，见 [LICENSE](LICENSE)。
