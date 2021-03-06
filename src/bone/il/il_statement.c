﻿#include "il_statement.h"
#include "../bone.h"
#include "../runtime/enviroment.h"
#include "il_lineno.h"
#include "il_stmt_all.h"

bnILStatement* bnNewILStatement(bnILStatementType type) {
        bnILStatement* ret = BN_MALLOC(sizeof(bnILStatement));
        ret->type = type;
        ret->line = -1;
        return ret;
}

void bnDumpILStatement(FILE* fp, struct bnStringPool* pool, bnILStatement* self,
                       int depth) {
        bnDumpLineno(fp, self->line);
        switch (self->type) {
                case BN_IL_STMT_NONE:
                        break;
                case BN_IL_STMT_EXPRSTMT:
                        bnDumpILStmtExpr(fp, pool, self->u.vExprStmt, depth);
                        break;
                case BN_IL_STMT_IF:
                        bnDumpILStmtIf(fp, pool, self->u.vIf, depth);
                        break;
                case BN_IL_STMT_IF_ELSE:
                        bnDumpILStmtIfElse(fp, pool, self->u.vIfElse, depth);
                        break;
                case BN_IL_STMT_WHILE:
                        bnDumpILStmtWhile(fp, pool, self->u.vWhile, depth);
                        break;
                case BN_IL_STMT_RETURN:
                        bnDumpILStmtReturn(fp, pool, self->u.vReturn, depth);
                        break;
                case BN_IL_STMT_SCOPE_INJECTION:
                        bnDumpILStmtScopeInjection(fp, pool, self->u.vScopeInj,
                                                   depth);
                        break;
                case BN_IL_STMT_DEFER:
                        bnDumpILStmtDefer(fp, pool, self->u.vDefer, depth);
                        break;
                case BN_IL_STMT_CONTINUE:
                        bnFindent(fp, depth);
                        fprintf(fp, "continue\n");
                        break;
                case BN_IL_STMT_BREAK:
                        bnFindent(fp, depth);
                        fprintf(fp, "break\n");
                        break;
                default:
                        assert(false);
                        break;
        }
}

void bnDeleteILStatement(bnILStatement* self) {
        if (self == NULL) {
                return;
        }
        switch (self->type) {
                case BN_IL_STMT_NONE:
                        break;
                case BN_IL_STMT_EXPRSTMT:
                        bnDeleteILStmtExpr(self->u.vExprStmt);
                        break;
                case BN_IL_STMT_IF:
                        bnDeleteILStmtIf(self->u.vIf);
                        break;
                case BN_IL_STMT_IF_ELSE:
                        bnDeleteILStmtIfElse(self->u.vIfElse);
                        break;
                case BN_IL_STMT_WHILE:
                        bnDeleteILStmtWhile(self->u.vWhile);
                        break;
                case BN_IL_STMT_RETURN:
                        bnDeleteILStmtReturn(self->u.vReturn);
                        break;
                case BN_IL_STMT_SCOPE_INJECTION:
                        bnDeleteILStmtScopeInjection(self->u.vScopeInj);
                        break;
                case BN_IL_STMT_DEFER:
                        bnDeleteILStmtDefer(self->u.vDefer);
                        break;
                case BN_IL_STMT_CONTINUE:
                        break;
                case BN_IL_STMT_BREAK:
                        break;
                default:
                        assert(false);
                        break;
        }
        BN_FREE(self);
}

void bnGenerateILStatement(struct bnInterpreter* bone, bnILStatement* self,
                           bnEnviroment* env, bnCompileCache* ccache) {
        switch (self->type) {
                case BN_IL_STMT_NONE:
                        break;
                case BN_IL_STMT_EXPRSTMT:
                        bnGenerateILStmtExpr(bone, self->u.vExprStmt, env,
                                             ccache);
                        break;
                case BN_IL_STMT_IF:
                        bnGenerateILStmtIf(bone, self->u.vIf, env, ccache);
                        break;
                case BN_IL_STMT_IF_ELSE:
                        bnGenerateILStmtIfElse(bone, self->u.vIfElse, env,
                                               ccache);
                        break;
                case BN_IL_STMT_WHILE:
                        bnGenerateILStmtWhile(bone, self->u.vWhile, env,
                                              ccache);
                        break;
                case BN_IL_STMT_RETURN:
                        bnGenerateILStmtReturn(bone, self->u.vReturn, env,
                                               ccache);
                        break;
                case BN_IL_STMT_SCOPE_INJECTION:
                        bnGenerateILStmtScopeInjection(bone, self->u.vScopeInj,
                                                       env, ccache);
                        break;
                case BN_IL_STMT_DEFER:
                        bnGenerateILStmtDefer(bone, self->u.vDefer, env,
                                              ccache);
                        break;
                case BN_IL_STMT_CONTINUE: {
                        bnLabel* start = bnPeekStack(ccache->whileStartStack);
                        start->refCount++;
                        bnWriteCode(env, BN_OP_GOTO);
                        bnWriteLabel(env, start);
                        g_ptr_array_add(env->labels, start);
                        break;
                }
                case BN_IL_STMT_BREAK: {
                        bnLabel* end = bnPeekStack(ccache->whileEndStack);
                        end->refCount++;
                        bnWriteCode(env, BN_OP_GOTO);
                        bnWriteLabel(env, end);
                        g_ptr_array_add(env->labels, end);
                        break;
                }
                default:
                        assert(false);
                        break;
        }
        bnAddLineRange(env, self->line);
}