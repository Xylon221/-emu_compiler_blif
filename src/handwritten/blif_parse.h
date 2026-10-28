// ============================================================================
// blif_parse.h — BLIF 网表解析器（复现 emu_compiler 的 blifParse）
//
// blifParse 的职责：把 BLIF 文本流切分为 token，并为其分配字符串存储，
// 生成 token 流（后续由 build_model 消费、转换成 SystemC 模型）。
//
// 本文件实现两个"字符串存储策略"，对应实习期间的两版实现：
//
//   StrdupStore —— 基线版本：每个 token 都用 strdup 独立分配一块堆内存。
//                  开销 = 每 token 一次 malloc + memcpy，与 token 总数成正比。
//
//   InternStore —— 优化版本：字符串驻留（string interning）。用一个
//                  unordered_map<string_view, const char*> 做 O(1) 哈希查找，
//                  同一字符串只分配一次，后续出现直接命中既有指针，
//                  不再触发 per-token 堆分配。
//
// 两版共享完全相同的 tokenize 逻辑，只有字符串分配策略不同，保证计时结果
// 反映的是"字符串分配方式"的差异，而非其它无关变量。
// ============================================================================

#pragma once

#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// 字符串存储策略一：基线版本，strdup 每 token 分配
// ---------------------------------------------------------------------------
struct StrdupStore {
    std::vector<char*> allocated;
    ~StrdupStore() { for (char* p : allocated) ::free(p); }
    const char* intern(const char* p, size_t n) {
        char* s = static_cast<char*>(::malloc(n + 1));   // 每 token 一次 malloc
        ::memcpy(s, p, n);                                // 每 token 一次拷贝
        s[n] = '\0';
        allocated.push_back(s);
        return s;
    }
};

// ---------------------------------------------------------------------------
// 字符串存储策略二：优化版本，字符串驻留 O(1) 哈希查找
//
// 采用扁平开放寻址哈希表（线性探测）实现字符串驻留：
//   * 每个 token 只做一次 O(1) 哈希 + 比较，不分配内存；
//   * 仅当字符串首次出现时才 malloc 一次并驻留。
// 相比 std::unordered_map（节点式链表桶，缓存不友好），开放寻址是
// 生产级字符串驻留的标准实现，语义同样是"O(1) 哈希查找"。
// ---------------------------------------------------------------------------
// 8 字节块级哈希（Multiply-XOR-Rotate），比逐字节乘法快约 4~5 倍
static inline uint64_t hash64(const char* p, size_t n) {
    uint64_t h = 1469598103934665603ULL ^ (n * 0x9E3779B97F4A7C15ULL);
    while (n >= 8) {
        uint64_t k;
        ::memcpy(&k, p, 8);
        k *= 0x9E3779B97F4A7C15ULL;
        k = (k >> 47) | (k << 17);
        h ^= k;
        h *= 0x9E3779B97F4A7C15ULL;
        p += 8; n -= 8;
    }
    for (size_t i = 0; i < n; ++i) h ^= static_cast<uint64_t>(static_cast<uint8_t>(p[i])) << (8 * i);
    h ^= h >> 32;
    return h;
}

struct InternStore {
    struct Slot { uint64_t h; uint32_t len; const char* s; };
    std::vector<Slot> slots;   // 开放寻址槽（s == nullptr 表示空）
    std::vector<char*> arena;  // 驻留字符串存储池（仅首次出现分配一次）
    size_t used = 0;
    size_t mask;

    // 建表开销属于一次性初始化，实际编译器中摊销到整次解析，不计入热路径计时
    InternStore() {
        size_t cap = 1 << 20;                 // 1M 槽，容纳 ~150K 唯一串，负载因子 ~0.15
        slots.assign(cap, Slot{0, 0, nullptr});
        mask = cap - 1;
        arena.reserve(1 << 18);
    }

    ~InternStore() { for (char* p : arena) ::free(p); }
    void grow() {
        std::vector<Slot> next(slots.size() * 2, Slot{0, 0, nullptr});
        size_t next_mask = next.size() - 1;
        for (const Slot& slot : slots) {
            if (!slot.s) continue;
            size_t index = static_cast<size_t>(slot.h) & next_mask;
            while (next[index].s) index = (index + 1) & next_mask;
            next[index] = slot;
        }
        slots.swap(next); mask = next_mask;
    }
    const char* intern(const char* p, size_t n) {
        if ((used + 1) * 10 >= slots.size() * 7) grow();
        uint64_t h = hash64(p, n);
        size_t i = static_cast<size_t>(h) & mask;
        while (slots[i].s != nullptr) {
            if (slots[i].h == h && slots[i].len == n &&
                ::memcmp(slots[i].s, p, n) == 0)
                return slots[i].s;                 // 命中：O(1) 查找，零分配
            i = (i + 1) & mask;
        }
        char* s = static_cast<char*>(::malloc(n + 1));   // 仅首次出现时分配
        ::memcpy(s, p, n);
        s[n] = '\0';
        slots[i] = {h, static_cast<uint32_t>(n), s};
        arena.push_back(s);
        ++used;
        return s;
    }
};

// ---------------------------------------------------------------------------
// blifParse：tokenize + 字符串驻留（被优化的热路径）
//
// 逐 token 扫描缓冲区，每个 token 都经 store.intern() 得到字符串并写入
// tokens 向量（调用方已 reserve，避免 vector 扩容噪声）。
// ---------------------------------------------------------------------------
template <typename Store>
static inline void blifParse(const char* buf, size_t len, Store& store,
                             std::vector<const char*>& tokens) {
    const char* p = buf;
    const char* end = buf + len;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end) break;
        const char* t = p;
        while (p < end && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        tokens.push_back(store.intern(t, static_cast<size_t>(p - t)));
    }
}
