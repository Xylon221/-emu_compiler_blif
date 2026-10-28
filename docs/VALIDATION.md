# 发布验证记录

日期：2026-10-28。WSL2 Ubuntu 22.04，Linux 6.6.87.2-microsoft-standard-WSL2，Intel Core i7-14700K；g++ 11.4.0，flex 2.6.4，bison 3.8.2，Python 3.10.12，C++17 `-O2`。在 Linux `/tmp` 新目录构建并重新生成 lexer/parser。

## 已执行

- `make -j2`：通过；flex/bison 从 PATH 发现。本机仅将原有工具安装目录加到 PATH，没有把该路径写死进发布 Makefile。
- `make test`：通过。0/1/1000 门输入、0/1/32 latch；两版模型摘要一致，手写 token 内容一致；非正轮数和错误语法拒绝。
- 存储测试：通过。验证非终止切片、重复字符串返回同一地址，740001 个唯一字符串触发扩容，扩容后旧指针稳定，reset 后可再次使用。
- `make bench`：通过。100000 门、2048 latch、737451 个空白分隔 token、4613.7 KiB；固定 seed=20260903；每版 7 轮。
- `make reference`：通过，手写 token 流逐项相等。

## 实际输出

| flex/bison | min | median | unique |
| --- | --- | --- | --- |
| strdup | 20.227 ms | 21.007 ms | 0（该字段不统计基线唯一数） |
| intern | 17.347 ms | 18.921 ms | 102152 |

最小耗时减少 14.24%，中位耗时减少 9.93%。两版摘要均为：

```text
model=dtp_top inputs=94 outputs=64 gates=100000 latches=2048 hash=fc470cd5625ed839
```

手写参考：基线 min/median=10.303/10.475 ms，驻留=7.834/8.061 ms；最小耗时减少约 24.0%。它在内存缓冲区上运行、表初始化不在热路径计时内，与 flex/bison 的读入及初始化口径不同。

原始输出见 [2026-10-28.log](validation/2026-10-28.log)。

## 发布时修正

- Makefile 默认使用 PATH 中的 flex/bison，支持显式覆盖，不依赖原电脑路径。
- 两版 reset 都释放字符串；驻留表释放后每轮重新建立；基线新增分配跟踪。生命周期管理会改变旧基准的成本。
- 驻留表支持负载阈值扩容，显式按长度复制并补零，避免固定表满后无限探测。
- 手写参考补齐字符串所有权，保留 token 内容副本，正确返回校验失败状态。
- 合成网表声明热控制输入，修正 `.latch D Q` 顺序；因此输入数从旧版本 66 变为 94，hash 也改变。
- 增加自动验证、存储单测、CI 配置及可选 Dockerfile。

## 结论边界

旧材料的 39% 和旧 flex/bison 的 26–27% 不是本版本结果。本次只测一类合成输入；不能外推到所有真实网表。模型摘要未覆盖完整电路语义。Docker 未在本机执行，GitHub Actions 状态应在上传后另行查看。
