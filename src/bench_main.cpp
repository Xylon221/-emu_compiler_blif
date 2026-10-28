// ============================================================================
// bench_main.cpp — BLIF 解析器基准驱动（chrono 细粒度计时）
//
// 计时 yyparse()（blif.y 生成的语法分析，内部调用 blif.l 生成的 yylex 词法）。
// 字符串存储策略由链接的 strstore 实现决定（strdup 基线 / 驻留优化）。
//
// 用法： ./blif_bench_<variant> <blif文件> [迭代次数]
// ============================================================================

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

extern "C" {
    int  yyparse(void);
    extern FILE* yyin;
    void model_reset(void);
    void model_print(void);
    void strstore_reset(void);
    unsigned long strstore_unique_count(void);
}

using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <blif_file> [iterations]\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const int M = (argc >= 3) ? std::atoi(argv[2]) : 7;

    if (M <= 0) { std::fprintf(stderr, "iterations must be positive\n"); return 2; }

    // 文件大小（信息展示）
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "open failed: %s\n", path); return 1; }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);

    std::vector<double> ts;
    int rc = 0;
    for (int i = 0; i < M; ++i) {
        model_reset();
        strstore_reset();
        yyin = std::fopen(path, "r");
        if (!yyin) { std::fprintf(stderr, "reopen failed\n"); return 1; }
        auto t0 = Clock::now();
        rc = yyparse();
        auto t1 = Clock::now();
        std::fclose(yyin);
        if (rc != 0) { std::fprintf(stderr, "yyparse failed (rc=%d)\n", rc); return 1; }
        ts.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(ts.begin(), ts.end());

    std::printf("文件: %s (%.1f KiB), 迭代: %d\n", path, double(sz) / 1024.0, M);
    std::printf("blifParse(yyparse)  min=%8.3f ms   med=%8.3f ms   unique=%lu\n",
                ts.front(), ts[ts.size() / 2], strstore_unique_count());
    model_print();
    strstore_reset();
    return 0;
}
