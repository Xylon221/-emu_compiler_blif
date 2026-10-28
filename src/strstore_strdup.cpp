// ============================================================================
// strstore_strdup.cpp — 基线实现：值传递 + strdup 语义
//
// 复刻原始低效路径：token 先构造成 std::string，再按值传递给存储函数；
// 存储函数内部再 strdup 一份 C 字符串。这样既包含参数值传递/对象拷贝成本，
// 也保留每 token 一次 malloc + 拷贝的字符串存储成本。
// ============================================================================
#include "strstore.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::vector<const char*> allocations;
const char* store_by_value(std::string token) {
    return ::strdup(token.c_str());
}
}

extern "C" const char* strstore_intern(const char* s, size_t n) {
    std::string token(s, n);
    const char* result = store_by_value(token);
    if (!result) std::abort();
    allocations.push_back(result);
    return result;
}

extern "C" void strstore_reset(void) {
    for (const char* p : allocations) std::free(const_cast<char*>(p));
    allocations.clear();
}

extern "C" unsigned long strstore_unique_count(void) {
    return 0;   // 基线无去重概念
}
