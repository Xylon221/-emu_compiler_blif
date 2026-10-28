// ============================================================================
// strstore_intern.cpp — 优化实现：字符串驻留（string interning）O(1) 哈希查找
//
// 采用扁平开放寻址哈希表（线性探测）：
//   * 每个 token 只做一次 O(1) 哈希 + 比较，不分配内存；
//   * 仅当字符串首次出现时才 malloc 一次并驻留。
// 相比 std::unordered_map（节点式链表桶，缓存不友好），开放寻址是生产级
// 字符串驻留的标准实现，语义同样是"O(1) 哈希查找"。
// ============================================================================
#include "strstore.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>

namespace {
struct Slot { uint64_t h; size_t len; const char* s; };

Slot*     g_slots = nullptr;
size_t    g_cap = 0;
size_t    g_mask = 0;
unsigned long g_used = 0;

inline uint64_t hash64(const char* p, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    while (n >= 8) {
        uint64_t k; ::memcpy(&k, p, 8);
        k *= 0x9E3779B97F4A7C15ULL; k = (k >> 47) | (k << 17);
        h ^= k; h *= 0x9E3779B97F4A7C15ULL;
        p += 8; n -= 8;
    }
    for (size_t i = 0; i < n; ++i) h ^= static_cast<uint64_t>(static_cast<uint8_t>(p[i])) << (8 * i);
    return h ^ (h >> 32);
}
void grow() {
    const size_t next_cap = g_cap ? g_cap * 2 : (1u << 20);
    Slot* next = static_cast<Slot*>(::calloc(next_cap, sizeof(Slot)));
    if (!next) std::abort();
    for (size_t old = 0; old < g_cap; ++old) {
        if (!g_slots[old].s) continue;
        size_t index = static_cast<size_t>(g_slots[old].h) & (next_cap - 1);
        while (next[index].s) index = (index + 1) & (next_cap - 1);
        next[index] = g_slots[old];
    }
    ::free(g_slots);
    g_slots = next; g_cap = next_cap; g_mask = next_cap - 1;
}

}

extern "C" const char* strstore_intern(const char* s, size_t n) {
    if (!g_slots || (g_used + 1) * 10 >= g_cap * 7) grow();
    uint64_t h = hash64(s, n);
    size_t i = static_cast<size_t>(h) & g_mask;
    while (g_slots[i].s != nullptr) {
        if (g_slots[i].h == h && g_slots[i].len == n && ::memcmp(g_slots[i].s, s, n) == 0)
            return g_slots[i].s;               // 命中：复用，零分配
        i = (i + 1) & g_mask;
    }
    char* c = static_cast<char*>(::malloc(n + 1));   // 仅首次分配
    if (!c) std::abort();
    ::memcpy(c, s, n);
    c[n] = 0;
    g_slots[i] = { h, n, c };
    ++g_used;
    return c;
}

extern "C" void strstore_reset(void) {
    for (size_t i = 0; i < g_cap; ++i) {
        ::free(const_cast<char*>(g_slots[i].s));
    }
    ::free(g_slots);
    g_slots = nullptr; g_cap = g_mask = 0;
    g_used = 0;
}

extern "C" unsigned long strstore_unique_count(void) {
    return g_used;
}
