#include "il_stmt_if.h"
#include "../runtime/enviroment.h"
#include "il_expression.h"
#include "il_statement.h"

bnILStmtIf* bnNewILStmtIf(bnILExpression* cond) {
        bnILStmtIf* ret = BN_MALLOC(sizeof(bnILStmtIf));
        ret->cond = cond;
        ret->statements = NULL;
        return ret;
}

void bnDumpILStmtIf(FILE* fp, struct bnStringPool* pool, bnILStmtIf* self,
                    int depth) {
        bnFindent(fp, depth);
        fprintf(fp, "If\n");
        bnDumpILExpression(fp, pool, self->cond, depth + 1);
        GList* iter = self->statements;
        while (iter != NULL) {
                bnDumpILStatement(fp, pool, iter->data, depth + 1);
                iter = iter->next;
        }
}

void bnGenerateILStmtIf(struct bnInterpreter* bone, bnILStmtIf* self,
                        bnEnviroment* env) {
        // generate condition
        bnGenerateILExpression(bone, self->cond, env);
        g_ptr_array_add(env->codeArray, BN_OP_GOTO_ELSE);
        bnLabel* ifFalse = bnGenerateLabel(env, -1);
        GList* iter = self->statements;
        while (iter != NULL) {
                bnGenerateILStatement(bone, iter->data, env);
                iter = iter->next;
        }
        ifFalse->pos = bnGenerateNOP(env);
}

void bnDeleteILStmtIf(bnILStmtIf* self) {
        bnDeleteILExpression(self->cond);
        g_list_free_full(self->statements, bnDeleteILStatement);
        BN_FREE(self);
}

bnILStmtIfElse* bnNewILStmtIfElse(bnILStmtIf* trueCase) {
        bnILStmtIfElse* ret = BN_MALLOC(sizeof(bnILStmtIfElse));
        ret->trueCase = trueCase;
        ret->statements = NULL;
        return ret;
}

void bnDumpILStmtIfElse(FILE* fp, struct bnStringPool* pool,
                        bnILStmtIfElse* self, int depth) {
        bnFindent(fp, depth);
        fprintf(fp, "if else\n");
        bnDumpILStmtIf(fp, pool, self->trueCase, depth + 1);
        GList* iter = self->statements;
        while (iter != NULL) {
                bnDumpILStatement(fp, pool, iter->data, depth + 1);
                iter = iter->next;
        }
}

void bnGenerateILStmtIfElse(struct bnInterpreter* bone, bnILStmtIfElse* self,
                            bnEnviroment* env) {
        // if(cond) { ... }
        bnGenerateILExpression(bone, self->trueCase->cond, env);
        g_ptr_array_add(env->codeArray, BN_OP_GOTO_ELSE);
        bnLabel* ifFalse = bnGenerateLabel(env, -1);
        GList* iter = self->trueCase->statements;
        while (iter != NULL) {
                bnGenerateILStatement(bone, iter->data, env);
                iter = iter->next;
        }
        g_ptr_array_add(env->codeArray, BN_OP_GOTO);
        bnLabel* ifTrue = bnGenerateLabel(env, -1);
        // if(cond) { ... } else { ... }
        ifFalse->pos = bnGenerateNOP(env);
        iter = self->statements;
        while (iter != NULL) {
                bnGenerateILStatement(bone, iter->data, env);
                iter = iter->next;
        }
        ifTrue->pos = bnGenerateNOP(env);
}

void bnDeleteILStmtIfElse(bnILStmtIfElse* self) {
        bnDeleteILStmtIf(self->trueCase);
        g_list_free_full(self->statements, bnDeleteILStatement);
        BN_FREE(self);
}