# BLIF 网表解析：字符串存储优化复现

使用同一套 flex/bison 解析器，对比“临时 `std::string` + 按值传递 + 每 token `strdup`”与“指针/长度传参 + 字符串驻留”。包含合成网表生成器、基准、自动验证及手写 tokenizer 参考实现。

本仓库复现网表编译器中解析阶段的性能优化方法，不是完整 emu_compiler 产品。当前语法用于受限 BLIF 子集的模型计数和信号名摘要，没有完整网表图、逻辑仿真、SystemC 代码生成或 Yosys 自动综合流程。

## 在另一台电脑复现

Linux 或 Windows WSL2，推荐 Ubuntu 22.04/24.04。在 Ubuntu 终端执行：

```bash
sudo apt-get update
sudo apt-get install -y git build-essential python3 flex bison
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/Xylon221/-emu_compiler_blif.git emu_compiler_blif
cd emu_compiler_blif
make -j2
make test
make bench
```

无需原电脑的 `/home/xylon/work/_tools`。默认使用 PATH 中的 flex/bison；特殊安装可用 `make FLEX=/path/to/flex BISON=/path/to/bison`。

`make test` 应完成存储表扩容校验、三种网表规模的模型摘要与 token 对齐校验、错误输入验证。性能差异没有固定通过阈值。

## 常用命令

| 命令 | 用途 |
| --- | --- |
| `make bench` | 生成 10 万门、2,048 latch 网表，两版各运行 7 轮 |
| `make bench GATES=20000 LATCHES=512` | 改变网表规模 |
| `make reference` | 运行手写 tokenizer 对照，不与 flex/bison 的绝对耗时混用 |
| `make test` | 模型摘要、token 内容、存储边界、扩容和错误输入验证 |
| `make clean` | 删除 `build/`，可用于验证重新生成解析器 |

自定义输入与轮数：

```bash
python3 scripts/gen_blif.py --gates 100000 --latches 2048 --seed 20260903 --out blif/dtp_synth.blif
./build/blif_bench_strdup blif/dtp_synth.blif 7
./build/blif_bench_intern blif/dtp_synth.blif 7
```

同一输入下两版的 `model=... inputs=... outputs=... gates=... latches=... hash=...` 应一致。该校验是摘要一致性，不证明所有 BLIF 语义等价；真值表内容没有纳入当前摘要。

## Docker 备选

```bash
docker build -t blif-bench .
docker run --rm blif-bench
```

镜像构建时自动执行测试。本次发布在 WSL 原生工具链验证，未在本机执行 Docker；APT 包未锁定补丁版本，目标是功能复现。

## 源码导航

```text
src/blif.l                flex 词法规则
src/blif.y                bison 简化语法、模型计数和信号名 hash
src/strstore.h            统一存储接口
src/strstore_strdup.cpp   基线字符串构造、拷贝和分配
src/strstore_intern.cpp   开放寻址驻留表、扩容、reset
src/bench_main.cpp        多轮 yyparse 计时
src/handwritten/         手写 tokenizer 参考
scripts/gen_blif.py       固定种子的合成 BLIF 生成器
scripts/verify.py         解析与对齐验证
tests/strstore_test.cpp   指针复用、长度、扩容和 reset 测试
docs/TECHNICAL_REVIEW.md  面试复盘、原理、性能口径与常见追问
docs/VALIDATION.md        当前版本实测记录
```

## 本次结果与项目关系

2026-10-28，i7-14700K / WSL2 / g++ 11.4 / `-O2`，10 万门合成网表、7 轮：最小耗时 **20.227 → 17.347 ms，耗时减少约 14.2%**；中位数 **21.007 → 18.921 ms，减少约 9.9%**。两版模型摘要相同。手写 tokenizer 本次耗时减少约 24.0%，其计时边界不同，不能用于替代 flex/bison 的结果。

旧工作材料中的 39% 与旧复现实验的 26–27% 不代表当前版本结果。当前版本修复了内存生命周期并调整了测试输入，详见 [验证记录](docs/VALIDATION.md)。

另一个项目 [端口更新优化](https://github.com/Xylon221/-dtp_ca_sim) 优化模型运行时调度；本仓库优化编译时解析。两者独立构建。

面试复盘入口：[技术复盘](docs/TECHNICAL_REVIEW.md)。
