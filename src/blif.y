/* ============================================================================
 * blif.y — BLIF 语法（bison）
 *
 * 对应 emu_compiler 的 BLIF 网表解析语法：把 token 流组装成网表模型。
 * 词法（tokenize + 字符串存储）在 blif.l 中完成，本文件负责语法结构。
 *
 * 生成： bison -d -o blif.tab.c blif.y   （-d 产出 blif.tab.h 供 blif.l 引用）
 * ========================================================================== */
%{
#include <stdio.h>
#include <string.h>
#include "strstore.h"

void yyerror(const char* s);
int yylex(void);   /* 由 flex 生成的 lex.yy.c 提供 */

/* 极简网表模型统计（用于两版功能对齐校验） */
static char g_model[256];
static int  g_inputs = 0, g_outputs = 0, g_gates = 0, g_latches = 0;
static unsigned long long g_hash = 1469598103934665603ULL;

void model_reset(void) {
    g_inputs = g_outputs = g_gates = g_latches = 0;
    g_model[0] = 0;
    g_hash = 1469598103934665603ULL;
}
static void model_note(const char* s) {
    for (; *s; ++s) { g_hash ^= (unsigned char)*s; g_hash *= 1099511628211ULL; }
}
void model_print(void) {
    printf("model=%s inputs=%d outputs=%d gates=%d latches=%d hash=%016llx\n",
           g_model, g_inputs, g_outputs, g_gates, g_latches,
           (unsigned long long)g_hash);
}
%}

%union { const char* s; }

%token <s> IDENT NUMBER
%token DOT_MODEL DOT_INPUTS DOT_OUTPUTS DOT_NAMES DOT_LATCH DOT_END

%%

file:
    stmts
  ;

stmts:
    %empty
  | stmts stmt
  ;

stmt:
    DOT_MODEL IDENT          { strncpy(g_model, $2, sizeof(g_model)-1); model_note($2); }
  | DOT_INPUTS inputs
  | DOT_OUTPUTS outputs
  | DOT_NAMES fanins cubes   { ++g_gates; }
  | DOT_LATCH latch_args     { ++g_latches; }
  | DOT_END                  { /* 结束 */ }
  ;

inputs:
    %empty
  | inputs IDENT             { ++g_inputs; model_note($2); }
  ;

outputs:
    %empty
  | outputs IDENT            { ++g_outputs; model_note($2); }
  ;

fanins:
    %empty
  | fanins IDENT             { model_note($2); }
  ;

cubes:
    %empty
  | cubes NUMBER NUMBER      { /* 真值表立方行：忽略语义，仅参与 token 计数 */ }
  ;

latch_args:
    %empty
  | latch_args IDENT         { model_note($2); }
  | latch_args NUMBER        { }
  ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "BLIF 语法错误: %s\n", s);
}
