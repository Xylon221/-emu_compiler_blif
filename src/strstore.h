// ============================================================================
// strstore.h — 字符串存储策略接口
//
// 词法扫描器（blif.l 生成的 yylex）对每个 token 调用 strstore_intern() 分配
// 字符串存储。链接不同的实现即可在"基线 / 优化"两版之间切换，而词法、语法
// 逻辑完全一致：
//
//   strstore_strdup.cpp —— 基线：strdup 语义，每 token 一次 malloc + 拷贝；
//   strstore_intern.cpp —— 优化：字符串驻留，O(1) 哈希查找，仅首次分配。
//
// 接口带显式长度 n（= yyleng），避免对已终止的 yytext 再跑一次 strlen。
// ============================================================================
#ifndef STRSTORE_H
#define STRSTORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 分配/驻留一个长度为 n 的字符串（语义同 strdup，但可去重）
const char* strstore_intern(const char* s, size_t n);

// 释放本轮字符串并重置存储；所有此前返回的指针失效。当前接口非线程安全。
void strstore_reset(void);

// 已驻留的唯一字符串数量（用于统计/校验）
unsigned long strstore_unique_count(void);

#ifdef __cplusplus
}
#endif

#endif
