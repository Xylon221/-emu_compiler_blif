/* ============================================================================
 * blif.l — BLIF 词法扫描器（flex）
 *
 * 对应 emu_compiler 中的 blifParse 词法部分：把 BLIF 文本流切分为 token，
 * 每个 token 经 strstore_intern() 分配字符串存储（strdup 基线 / 驻留优化）。
 * 由 bison 的 blif.y 语法规则调用（yyparse -> yylex）。
 *
 * 生成： flex -o lex.yy.c blif.l
 * ========================================================================== */
%option noyywrap
%option nounput
%option noinput
%option full
%option 8bit

%{
#include <stdlib.h>
#include "blif.tab.h"      /* bison 生成的 token 定义 */
#include "strstore.h"      /* 字符串存储策略（strdup / 驻留） */
%}

%%
[ \t\r\n]+              ;                       /* 忽略空白 */

\.model                 return DOT_MODEL;
\.inputs                return DOT_INPUTS;
\.outputs               return DOT_OUTPUTS;
\.names                 return DOT_NAMES;
\.latch                 return DOT_LATCH;
\.end                   return DOT_END;

[01-]+                  { yylval.s = strstore_intern(yytext, yyleng); return NUMBER; }
[A-Za-z0-9_$]+          { yylval.s = strstore_intern(yytext, yyleng); return IDENT; }

.                       ;                       /* 其它字符忽略 */
<<EOF>>                 { return 0; }

%%
