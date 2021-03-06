﻿#include "interpreter.h"
#include "../il/il_toplevel.h"
#include "../parse/ast.h"
#include "../parse/ast2il.h"
#include "../parse/parser.h"
#include "../util/io.h"
#include "array.h"
#include "bool.h"
#include "enviroment.h"
#include "extern/dir.h"
#include "extern/file.h"
#include "extern/reflection.h"
#include "extern/regex.h"
#include "extern/string_buffer.h"
#include "extern/system.h"
#include "frame.h"
#include "heap.h"
#include "integer.h"
#include "lambda.h"
#include "module.h"
#include "object.h"
#include "std.h"
#include "string.h"
#include "vm.h"

static void free_gstr(void* v);
static void load_plugins(bnInterpreter* self, const char* currentdir);
static void load_plugin(bnInterpreter* self, gchar* path);
static void unload_plugins(bnInterpreter* self);
static bool is_dll(gchar* path);
static void show_stack_trace(bnInterpreter* self);

bnInterpreter* bnNewInterpreter(const char* filenameRef, int argc,
                                char* argv[]) {
        bnInterpreter* ret = BN_MALLOC(sizeof(bnInterpreter));
        ret->filenameRef = filenameRef;
        ret->plugins = NULL;
        ret->pool = bnNewStringPool();
        ret->heap = bnNewHeap();
        ret->argc = argc;
        ret->argv = g_ptr_array_new();
        ret->frame = NULL;
        ret->externTable =
            g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
        ret->sharedTable =
            g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
        ret->__jstack = bnNewJStack();
        ret->callStack = bnNewStack();
        if (ret->argc > 1) {
                for (int i = 0; i < ret->argc; i++) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
                        g_ptr_array_add(ret->argv,
                                        (gpointer)bnIntern(ret->pool, argv[i]));
#pragma clang diagnostic pop
                }
        }
#if DEBUG
        ret->out = bnStdout();
        ret->err = bnStderr();
        ret->in = bnStdin();
#endif
        bnExternSystem(ret);
        bnExternFile(ret);
        bnExternDir(ret);
        bnExternReflection(ret);
        bnExternStringBuffer(ret);
        bnExternRegex(ret);
        return ret;
}

void bnLink(bnInterpreter* bone, const char* path) { load_plugins(bone, path); }

int bnEval(bnInterpreter* self) {
        GString* resolved = bnResolveLoadPath(self->filenameRef);
        if (!bnExists(resolved->str)) {
                g_string_free(resolved, TRUE);
                printf("abort:`%s` is not found", self->filenameRef);
                return 1;
        }
        bnAST* ret = bnParseFile(self->pool, resolved->str);
        if (ret == NULL) {
                //構文解析に失敗したので終了
                printf("abort:syntax error in `%s`", self->filenameRef);
                g_string_free(resolved, TRUE);
                return 1;
        }
        g_string_free(resolved, TRUE);
        //中間表現に変換
        bnILToplevel* iltop = bnAST2IL(ret);
        //コード生成して実行
        bnEnviroment* env =
            bnNewEnviroment(bnIntern(self->pool, self->filenameRef));
        self->frame = bnNewFrame();
        bnWriteBuiltin(self, self->frame, self->pool);
        bnGenerateILTopLevel(self, iltop, env);
        bnWriteCode(env, BN_OP_DEFER_NEXT);
        bnDeleteAST(ret);
        bnExecute(self, env, self->frame);
        show_stack_trace(self);
        int status = self->frame->panic ? 1 : 0;
        self->frame->panic = NULL;
        bnDeleteILTopLevel(iltop);
        bnDeleteEnviroment(env);
        bnGC(self);
        bnDeleteFrame(self->frame);
        self->frame = NULL;
        g_hash_table_remove_all(self->externTable);
        bnGC(self);
        unload_plugins(self);
        return status;
}

void bnWriteBuiltin(bnInterpreter* self, bnFrame* frame,
                    struct bnStringPool* pool) {
        bnReference t = bnNewBool(self, true);
        bnReference f = bnNewBool(self, false);
        bnWriteVariable2(frame, pool, "true", t);
        bnWriteVariable2(frame, pool, "false", f);
#if DEBUG
        bnWriteVariable2(
            frame, pool, "assert",
            bnNewLambdaFromCFunc(self, bnStdDebugAssert, pool, BN_C_ADD_PARAM,
                                 "cond", BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "die",
            bnNewLambdaFromCFunc(self, bnStdDebugDie, pool, BN_C_ADD_EXIT));

        bnWriteVariable2(
            frame, pool, "print",
            bnNewLambdaFromCFunc(self, bnStdDebugPrint, pool, BN_C_ADD_PARAM,
                                 "str", BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "println",
            bnNewLambdaFromCFunc(self, bnStdDebugPrintln, pool, BN_C_ADD_PARAM,
                                 "str", BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "debugBreak",
            bnNewLambdaFromCFunc(self, bnStdDebugBreak, pool, BN_C_ADD_EXIT));

        bnWriteVariable2(frame, pool, "dumpTable",
                         bnNewLambdaFromCFunc(self, bnStdDebugDumpTable, pool,
                                              BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "showInfo",
            bnNewLambdaFromCFunc(self, bnStdDebugShowInfo, pool, BN_C_ADD_PARAM,
                                 "obj", BN_C_ADD_EXIT));

#endif
        bnWriteVariable2(frame, pool, "argc",
                         bnNewInteger(self, BN_MAX(self->argc - 2, 0)));
        bnReference argvRef = bnNewArray(self, BN_MAX(self->argc - 2, 0));
        bnObject* argv = bnGetObject(self->heap, argvRef);
        for (int i = 2; i < self->argc; i++) {
                bnSetArrayElementAt(
                    argv, i - 2,
                    bnNewString(
                        self, (bnStringView)g_ptr_array_index(self->argv, i)));
        }
        bnWriteVariable2(frame, pool, "argv", argvRef);
        bnWriteVariable2(
            frame, pool, "object",
            bnNewLambdaFromCFunc(self, bnStdSystemObject, pool, BN_C_ADD_RETURN,
                                 "ret", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "string",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemString, pool, BN_C_ADD_PARAM,
                             "array", BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "array",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemArray, pool, BN_C_ADD_PARAM,
                             "length", BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "include",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemInclude, pool, BN_C_ADD_PARAM,
                             "path", BN_C_ADD_RETURN, "...", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "load",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemLoad, pool, BN_C_ADD_PARAM,
                             "path", BN_C_ADD_RETURN, "...", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "eval",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemEval, pool, BN_C_ADD_PARAM,
                             "path", BN_C_ADD_RETURN, "...", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "extern_var",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemExternVar, pool, BN_C_ADD_PARAM,
                             "name", BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        bnWriteVariable2(frame, pool, "extern_def",
                         bnNewLambdaFromCFunc(
                             self, bnStdSystemExternDef, pool, BN_C_ADD_PARAM,
                             "name", BN_C_ADD_PARAM, "params", BN_C_ADD_PARAM,
                             "returns", BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "panic",
            bnNewLambdaFromCFunc(self, bnStdSystemPanic, pool, BN_C_ADD_PARAM,
                                 "error", BN_C_ADD_EXIT));
        bnWriteVariable2(
            frame, pool, "recover",
            bnNewLambdaFromCFunc(self, bnStdSystemRecover, pool,
                                 BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        //カレントワーキングディレクトリ
        gchar* cwd = g_get_current_dir();
        bnWriteVariable2(frame, pool, "CWD", bnNewString2(self, cwd));
        g_free(cwd);
        // bone実行ファイルのディレクトリ
        GString* asmPath = bnGetExecutableFileDir();
        bnWriteVariable2(frame, pool, "DIR", bnNewString2(self, asmPath->str));
        g_string_free(asmPath, TRUE);
}

void bnFormatThrow(bnInterpreter* self, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        bnVFormatThrow(self, fmt, ap);
        va_end(ap);
}

void bnVFormatThrow(bnInterpreter* self, const char* fmt, va_list ap) {
        char buf[512];
        vsprintf(buf, fmt, ap);
        bnReference ref = bnNewString(self, bnIntern(self->pool, buf));
        bnThrow(self, ref, BN_JMP_CODE_EXCEPTION);
}

void bnThrow(bnInterpreter* self, bnReference exception, int code) {
        bnPanic(self, exception);
        BN_JMP_DO(self->__jstack, code);
}

void bnPanic(bnInterpreter* self, bnReference exception) {
        bnFrame* iter = self->frame;
        while (1) {
                if (iter->next == NULL) {
                        break;
                }
                iter = iter->next;
        }
        iter->panic = exception;
}

bnObject* bnTypeAssertFunc(bnInterpreter* bone, const char* paramName,
                           bnObject* obj, bnObjectType expect,
                           const char* optExtensionName, const char* filename,
                           int lineno) {
        if (expect != BN_OBJECT_ANY && expect == obj->type) {
                return obj;
        }
        if (expect == BN_OBJECT_ANY) {
                bnStringView a = ((bnAny*)obj)->type;
                bnStringView b = bnIntern(bone->pool, optExtensionName);
                if (a == b) {
                        return obj;
                } else {
                        bnFormatThrow(bone, "should be `%s` is %s :%s<%d>",
                                      paramName, optExtensionName, filename,
                                      lineno);
                }
        }
        switch (expect) {
                case BN_OBJECT_PROTO: {
                        bnFormatThrow(bone,
                                      "should be `%s` is plain object :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_INTEGER: {
                        bnFormatThrow(bone, "should be `%s` is integer :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_DOUBLE: {
                        bnFormatThrow(bone, "should be `%s` is double :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_CHAR: {
                        bnFormatThrow(bone, "should be `%s` is char :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_STRING: {
                        bnFormatThrow(bone, "should be `%s` is string :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_BOOL: {
                        bnFormatThrow(bone, "should be `%s` is bool :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_LAMBDA: {
                        bnFormatThrow(bone, "should be `%s` is lambda :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_ARRAY: {
                        bnFormatThrow(bone, "should be `%s` is array :%s<%d>",
                                      paramName, filename, lineno);
                }
                case BN_OBJECT_ANY: {
                        bnFormatThrow(bone, "should be `%s` is any :%s<%d>",
                                      paramName, filename, lineno);
                }
        }
        return obj;
}

bnReference bnGetBool(struct bnStringPool* pool, bnFrame* frame, bool cond) {
        bnReference ret =
            cond ? bnGetTrue(pool, frame) : bnGetFalse(pool, frame);
        assert(ret != NULL);
        return ret;
}

bnReference bnGetTrue(struct bnStringPool* pool, bnFrame* frame) {
        return bnReadVariable2(frame, pool, "true");
}

bnReference bnGetFalse(struct bnStringPool* pool, bnFrame* frame) {
        return bnReadVariable2(frame, pool, "false");
}

void bnWriteExtern(bnInterpreter* self, bnStringView name, bnReference ref) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
        assert(ref != NULL);
        g_hash_table_replace(self->externTable, (gpointer)name, (gpointer)ref);
#pragma clang diagnostic pop
}

void bnWriteExtern2(bnInterpreter* self, const char* str, bnReference ref) {
        bnWriteExtern(self, bnIntern(self->pool, str), ref);
}

bnReference bnReadExtern(bnInterpreter* self, bnStringView name) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
        return (bnReference)g_hash_table_lookup(self->externTable,
                                                (gpointer)name);
#pragma clang diagnostic pop
}

bnReference bnReadExtern2(bnInterpreter* self, const char* str) {
        return bnReadExtern(self, bnIntern(self->pool, str));
}

bool bnIsShared(bnInterpreter* self, bnStringView name) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
        return g_hash_table_contains(self->sharedTable, (gpointer)name);
#pragma clang diagnostic pop
}

bool bnIsShared2(bnInterpreter* self, const char* str) {
        return bnIsShared(self, bnIntern(self->pool, str));
}

bnReference bnAddShared(bnInterpreter* self, bnStringView name,
                        bnReference ref) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
        g_hash_table_replace(self->sharedTable, (gpointer)name, ref);
#pragma clang diagnostic pop
        return ref;
}

bnReference bnAddShared2(bnInterpreter* self, const char* str,
                         bnReference ref) {
        bnAddShared(self, bnIntern(self->pool, str), ref);
        return ref;
}

bnReference bnGetShared(bnInterpreter* self, bnStringView name) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
        return g_hash_table_lookup(self->sharedTable, (gpointer)name);
#pragma clang diagnostic pop
}

bnReference bnGetShared2(bnInterpreter* self, const char* str) {
        return bnGetShared(self, bnIntern(self->pool, str));
}

void bnDeleteInterpreter(bnInterpreter* self) {
        //あらゆるルートを参照不可にしてから
        //ガベージコレクションを実施
        self->frame = NULL;
        g_hash_table_remove_all(self->sharedTable);
        g_hash_table_remove_all(self->externTable);
        bnGC(self);
        bnDeleteStringPool(self->pool);
        bnDeleteHeap(self->heap);
        bnDeleteJStack(self->__jstack);
        bnDeleteStack(self->callStack, free_gstr);
        g_hash_table_destroy(self->sharedTable);
        g_hash_table_destroy(self->externTable);
        g_ptr_array_unref(self->argv);
        BN_FREE(self);
}

static void free_gstr(void* v) { g_string_free(v, TRUE); }

static void load_plugins(bnInterpreter* self, const char* currentdir) {
        GDir* dir = g_dir_open(currentdir, 0, NULL);
        //ディレクトリを開けた場合
        if (!dir) {
                return;
        }
        const gchar* name;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
        while (name = g_dir_read_name(dir)) {
                gchar* path;
                path = g_build_filename(currentdir, name, NULL);
                if (g_file_test(path, G_FILE_TEST_IS_DIR) || !is_dll(path) ||
                    strstr(path, "libbone")) {
                        g_free(path);
                        continue;
                }
                load_plugin(self, path);
                g_free(path);
        }
#pragma clang diagnostic pop
        g_dir_close(dir);
}

static void load_plugin(bnInterpreter* self, gchar* path) {
        bnModule* mod = bnNewModule(path);
        //プラグインの要求するバージョンと、
        //実際に動作するバージョンが一致するか確認する
        bnPluginGetTargetVersion getTargetVersion =
            (bnPluginGetTargetVersion)bnGetSymbol(mod, "GetTargetVersion");
        if (getTargetVersion == NULL) {
                fprintf(stderr, "%s is not defined xxx_GetTargetVersion\n",
                        path);
                bnDeleteModule(mod);
                return;
        }
        const char* versionString = (const char*)getTargetVersion();
        if (strcmp(versionString, bnGetBuildVersion())) {
                fprintf(stderr, "not match build version: %s\n", path);
                fprintf(stderr, "required version: %s\n", versionString);
                fprintf(stderr, "real     version: %s\n", bnGetBuildVersion());
                bnDeleteModule(mod);
                return;
        }
        //初期化関数を呼び出す
        bnPluginInit init = (bnPluginInit)bnGetSymbol(mod, "Init");
        if (init != NULL) {
                init(self);
        }
        self->plugins = g_list_append(self->plugins, mod);
}

static void unload_plugins(bnInterpreter* self) {
        GList* iter = self->plugins;
        while (iter != NULL) {
                bnModule* mod = iter->data;
                bnPluginDestroy destroy =
                    (bnPluginDestroy)bnGetSymbol(mod, "Destroy");
                if (destroy != NULL) {
                        destroy(self);
                }
                bnDeleteModule(mod);
                iter->data = NULL;
                iter = iter->next;
        }
        g_list_free(self->plugins);
        self->plugins = NULL;
}

static bool is_dll(gchar* path) {
#if __APPLE__
        return g_str_has_suffix(path, ".dylib");
#elif _WIN32
        return g_str_has_suffix(path, ".dll");
#elif __linux__
        return g_str_has_suffix(path, ".so");
#else
        return false;
#endif
}

static void show_stack_trace(bnInterpreter* self) {
        while (bnGetStackSize(self->callStack) > 0) {
                GString* gbuf = bnPopStack(self->callStack);
                if (self->frame->panic != NULL) {
                        printf("TRACE: %s\n", gbuf->str);
                }
                g_string_free(gbuf, TRUE);
        }
        if (self->frame->panic != NULL) {
                printf("panic:");
                bnPrintObject(stdout, self,
                              bnGetObject(self->heap, self->frame->panic));
                printf("\n");
        }
}