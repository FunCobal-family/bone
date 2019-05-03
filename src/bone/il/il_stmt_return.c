#include "il_stmt_return.h"

bnILStmtReturn* bnNewILStmtReturn(bnILExpression* expr) {
        bnILStmtReturn* ret = BN_MALLOC(sizeof(bnILStmtReturn));
        ret->expr = expr;
        return ret;
}

void bnDumpILStmtReturn(FILE* fp, struct bnStringPool* pool,
                        bnILStmtReturn* self, int depth) {
        bnFindent(fp, depth);
        fprintf(fp, "return\n");
        if (self->expr != NULL) {
                bnDumpILExpression(fp, pool, self->expr, depth + 1);
        }
}

void bnGenerateILStmtReturn(struct bnInterpreter* bone, bnILStmtReturn* self,
                            struct bnEnviroment* env) {}

void bnDeleteILStmtReturn(bnILStmtReturn* self) {
        bnDeleteILExpression(self->expr);
        BN_FREE(self);
}