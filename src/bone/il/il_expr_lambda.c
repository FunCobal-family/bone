#include "il_expr_lambda.h"
#include "il_statement.h"

bnILExprLambda* bnNewILExprLambda() {
        bnILExprLambda* ret = BN_MALLOC(sizeof(bnILExprLambda));
        ret->parameters = NULL;
        ret->returns = NULL;
        ret->statements = NULL;
        return ret;
}

void bnDumpILExprLambda(FILE* fp, bnILExprLambda* self, int depth) {
        bnFindent(fp, depth);
        fprintf(fp, "lambda\n");
        // parameters
        bnFindent(fp, depth + 1);
        fprintf(fp, "parameters\n");
        GList* iter = self->parameters;
        while (iter != NULL) {
                GString* param = iter->data;
                bnFindent(fp, depth + 2);
                fprintf(fp, "%s\n", param->str);
                iter = iter->next;
        }
        // returns
        bnFindent(fp, depth + 1);
        fprintf(fp, "returns\n");
        iter = self->returns;
        while (iter != NULL) {
                GString* param = iter->data;
                bnFindent(fp, depth + 2);
                fprintf(fp, "%s\n", param->str);
                iter = iter->next;
        }
        // statements
        bnFindent(fp, depth + 1);
        fprintf(fp, "statements\n");
        iter = self->statements;
        while (iter != NULL) {
                bnILStatement* stmt = iter->data;
                bnDumpILStatement(fp, stmt, depth + 2);
                iter = iter->next;
        }
}

void bnDeleteILExprLambda(bnILExprLambda* self) {}