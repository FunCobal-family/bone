#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../bone.h"

static bnParserInputTag gParserInputTag;
static int gLine = -1;
static int gColumn = -1;
static GString *gStr = NULL;
static void init_string_lit();

bnAST *bnParseFile(const char *filename) {
        gParserInputTag = BN_PARSER_INPUT_FROM_FILE;
        extern FILE *yyin;
        extern void yy_calc_start(void);
        extern int yyparse(void);
        extern bnAST *yy_release();
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
                perror("bnParseFile");
                return NULL;
        }
        yy_calc_start();
        yyin = fp;
        if (yyparse()) {
                //失敗
                return NULL;
        }
        return yy_release();
}

bnAST *bnParseString(const char *source) {
        gParserInputTag = BN_PARSER_INPUT_FROM_SOURCE;
        extern void yy_setstr(char *source);
        extern void yy_clearstr();
        extern void yy_calc_start(void);
        extern int yyparse(void);
        extern bnAST *yy_release();
        yy_calc_start();
        yy_setstr(strdup(source));
        if (yyparse()) {
                yy_clearstr();
                return NULL;
        }
        yy_clearstr();
        return yy_release();
}

bnParserInputTag bnGetParserInputTag() { return gParserInputTag; }

void bnBeginStringLit() { init_string_lit(); }

void bnAppendStringLit(char c) {
        assert(gStr != NULL);
        g_string_append_c(gStr, c);
}

bnAST *bnEndStringLit() {
        bnAST *ret = bnNewStringAST(gStr);
        gStr = NULL;
        return ret;
}

void bnSetParseLine(int line) { gLine = line; }

int bnGetParseLine() { return gLine; }

void bnSetParseColumn(int column) { gColumn = column; }

int bnGetParseColumn() { return gColumn; }

// private
static void init_string_lit() {
        assert(gStr == NULL);
        gStr = g_string_new(NULL);
}