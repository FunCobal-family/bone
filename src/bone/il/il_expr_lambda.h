#ifndef BONE_IL_ILEXPRLAMBDA_H
#define BONE_IL_ILEXPRLAMBDA_H
#include "../bone.h"
#include "compile_cache.h"
struct bnEnviroment;
struct bnInterpreter;

/**
 * bnILExprLambda is lambda.
 */
typedef struct bnILExprLambda {
        GPtrArray* parameters;
        GPtrArray* returns;
        GPtrArray* statements;
        bnStringView filename;
        int lineno;
} bnILExprLambda;

/**
 * return new instance of bnILExprLambda.
 * @param filename
 * @param lineno
 * @return
 */
bnILExprLambda* bnNewILExprLambda(bnStringView filename, int lineno);

/**
 * print a information of bnILExprLambda.
 * @param fp
 * @param pool
 * @param self
 * @param depth
 */
void bnDumpILExprLambda(FILE* fp, struct bnStringPool* pool,
                        bnILExprLambda* self, int depth);

void bnGenerateILExprLambda(struct bnInterpreter* bone, bnILExprLambda* self,
                            struct bnEnviroment* env, bnCompileCache* ccache);

/**
 * free a bnILExprLambda.
 * @param self
 */
void bnDeleteILExprLambda(bnILExprLambda* self);
#endif
