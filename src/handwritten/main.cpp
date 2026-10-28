// ============================================================================
// main.cpp — BLIF 解析器性能对比驱动（chrono 细粒度计时）
//
// 复现目标：用 std::chrono 细粒度计时定位 blifParse 为网表解析瓶颈，
//           量化 strdup 每 token 分配 → 字符串驻留 O(1) 查找的优化收益。
//
// 用法： ./blif_bench <blif文件> [迭代次数]
// ============================================================================

#include "blif_parse.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

using Clock = std::chrono::steady_clock;

// 读入整个文件；记录读取耗时（用于计算 blifParse 占网表解析的比例）
static std::vector<char> read_file(const char* path, double* ms_read) {
    auto t0 = Clock::now();
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "open failed: %s\n", path); std::exit(1); }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<char> buf(static_cast<size_t>(sz) + 1);
    if (sz) { size_t r = std::fread(buf.data(), 1, static_cast<size_t>(sz), f); (void)r; }
    buf[static_cast<size_t>(sz)] = '\0';
    std::fclose(f);
    auto t1 = Clock::now();
    *ms_read = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buf;
}

// 统计 token 总数（单独一趟，不计入 blifParse 计时）
static size_t count_tokens(const char* buf, size_t len) {
    size_t n = 0;
    bool in = false;
    for (size_t i = 0; i < len; ++i) {
        char c = buf[i];
        bool ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (!ws && !in) { in = true; ++n; }
        else if (ws) in = false;
    }
    return n;
}

// 从 token 流做轻量统计（不计时），证明 blifParse 产出了真实网表结构
static void summarize(const std::vector<std::string>& tk) {
    size_t n_model = 0, n_names = 0, n_latch = 0, n_end = 0;
    const char* model = "";
    for (size_t i = 0; i < tk.size(); ++i) {
        const char* t = tk[i].c_str();
        if (t[0] == '.') {
            if (std::strcmp(t, ".model") == 0 && i + 1 < tk.size()) { ++n_model; model = tk[i + 1].c_str(); }
            else if (std::strcmp(t, ".names") == 0) ++n_names;
            else if (std::strcmp(t, ".latch") == 0) ++n_latch;
            else if (std::strcmp(t, ".end") == 0) ++n_end;
        }
    }
    std::printf("  网表摘要: model=%s, .names 门=%zu, .latch=%zu, .end=%zu\n",
                model, n_names, n_latch, n_end);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <blif_file> [iterations]\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const int M = (argc >= 3) ? std::atoi(argv[2]) : 7;

    if (M <= 0) return 2;
    double ms_read = 0;
    std::vector<char> buf = read_file(path, &ms_read);
    size_t len = buf.size() - 1;
    const char* data = buf.data();
    size_t n_tokens = count_tokens(data, len);

    std::printf("== BLIF 网表解析性能对比 ==\n");
    std::printf("文件: %s\n", path);
    std::printf("大小: %.2f KiB, token 数: %zu, 迭代: %d\n\n",
                double(len) / 1024.0, n_tokens, M);

    // ---- 两版分别计时 ----
    struct Result { double min_ms; double med_ms; std::vector<std::string> tokens; };
    auto bench = [&](auto make_store) -> Result {
        std::vector<double> ts;
        std::vector<std::string> keep;
        for (int i = 0; i < M; ++i) {
            std::vector<const char*> tokens;
            tokens.reserve(n_tokens);
            auto store = make_store();
            auto t0 = Clock::now();
            blifParse(data, len, store, tokens);
            auto t1 = Clock::now();
            ts.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            if (i == M - 1) {
                keep.reserve(tokens.size());
                for (const char* token : tokens) keep.emplace_back(token);
            }
        }
        std::sort(ts.begin(), ts.end());
        return { ts.front(), ts[ts.size() / 2], std::move(keep) };
    };

    Result base = bench([] { return StrdupStore{}; });
    Result opt  = bench([] { return InternStore{}; });

    // ---- 功能对齐校验：两版 token 流必须逐字节一致 ----
    bool eq = (base.tokens.size() == opt.tokens.size());
    if (eq) {
        for (size_t i = 0; i < base.tokens.size(); ++i)
            if (base.tokens[i] != opt.tokens[i]) { eq = false; break; }
    }
    std::printf("功能对齐校验: %s（token 数=%zu，两版逐 token 字符串完全一致）\n",
                eq ? "PASS" : "FAIL", base.tokens.size());
    summarize(base.tokens);
    std::printf("\n");

    // ---- 结果输出 ----
    double t_base = base.min_ms;
    double t_opt = opt.min_ms;
    double speedup = t_base / t_opt;
    double improve = (t_base - t_opt) / t_base * 100.0;
    double parse_share = t_base / (ms_read + t_base) * 100.0;

    std::printf("---- 计时结果（取 %d 次最小值，单位 ms）----\n", M);
    std::printf("  文件读取                 : %8.3f ms\n", ms_read);
    std::printf("  blifParse (strdup 基线)  : %8.3f ms   (中位 %8.3f)\n", base.min_ms, base.med_ms);
    std::printf("  blifParse (驻留 优化)    : %8.3f ms   (中位 %8.3f)\n", opt.min_ms, opt.med_ms);
    std::printf("\n");
    std::printf("  blifParse 占网表解析比例  : %.1f%%  (读 %.3f ms + 解析 %.3f ms)\n",
                parse_share, ms_read, t_base);
    std::printf("  blifParse 提速            : %.2f x\n", speedup);
    std::printf("  blifParse 性能提升        : %.1f %%\n", improve);

    return eq ? 0 : 1;
}
