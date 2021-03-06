﻿#include "test.h"
#include <assert.h>
#include <string.h>
#include "bone.h"
#include "il/il_toplevel.h"
#include "parse/ast2il.h"
#include "parse/parser.h"
#include "runtime/enviroment.h"
#include "runtime/frame.h"
#include "runtime/integer.h"
#include "runtime/interpreter.h"
#include "runtime/object.h"
#include "runtime/opcode.h"
#include "runtime/vm.h"
#include "util/args.h"
#include "util/getline.h"
#include "util/string_pool.h"
#include "util/string_util.h"

#if !defined(_WIN32)
#define EXCHANGE_STDOUT (1)
#endif

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

static int string_compare(const void* a, const void* b) {
        return strcmp((const char*)a, (const char*)b);
}

static GList* get_files(const char* dir) {
        GString* exeDir = bnGetExecutableFileDir();
        gchar* fullDir = g_build_filename(exeDir->str, dir, NULL);
        GError* err = NULL;
        GDir* dirp = g_dir_open(fullDir, 0, &err);
        if (dirp == NULL) {
                printf("%s\n", err->message);
                g_string_free(exeDir, TRUE);
                g_free(fullDir);
                return NULL;
        }
        const gchar* file = ".";
        GList* ret = NULL;
        while ((file = g_dir_read_name(dirp)) != NULL) {
                gchar* path = g_build_filename(fullDir, file, NULL);
                ret = g_list_append(ret, path);
        }
        ret = g_list_sort(ret, string_compare);
        g_dir_close(dirp);
        g_free(fullDir);
        g_string_free(exeDir, TRUE);
        return ret;
}

static void xremove(const gchar* path) {
#if defined(__APPLE__) || defined(__linux__)
        remove(path);
#else
        g_remove(path);
#endif
}

static void writeEnv(const gchar* out, struct bnStringPool* pool,
                     bnEnviroment* env) {
        if (env == NULL) {
                return;
        }
        FILE* fp = fopen(out, "w");
        if (fp == NULL) {
                perror("writeEnv");
                return;
        }
        int pos = 0;
        int len = env->codeArray->len;
        while (pos < len) {
                pos = bnPrintOpcode(fp, pool, env, pos);
                fprintf(fp, "\n");
        }
        fclose(fp);
}

static void writeIL(const gchar* out, struct bnStringPool* pool,
                    bnILToplevel* il) {
        if (il == NULL) {
                return;
        }
        FILE* fp = fopen(out, "w");
        if (fp == NULL) {
                perror("writeIL");
                return;
        }
        bnDumpILTopLevel(fp, pool, il, 0);
        fclose(fp);
}

static void writeFile(const gchar* out) {
        FILE* fp = fopen(out, "r");
        if (fp == NULL) {
                perror("writeFile");
                return;
        }
        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = bnGetLine(&line, &len, fp)) != -1) {
                printf("    %s", line);
        }
        free(line);
        fclose(fp);
}

static void string_destroy(gpointer data) {
        GString* str = data;
        g_string_free(str, TRUE);
}

typedef enum test_mask {
        test_mask_parse = 1 << 0,
        test_mask_vm = 1 << 1,
        test_mask_run = 1 << 2,
        test_mask_panic = 1 << 3,
        test_mask_expect_pass = 1 << 4,
        test_mask_expect_fail = 1 << 5,
} test_mask;

typedef enum test_result {
        test_result_pass,
        test_result_fail,
        test_result_unknown,
} test_result;

static test_result test_check_parse(const char* testDir, const char* testName,
                                    const gchar* path, int flags) {
        struct bnStringPool* pool = bnNewStringPool();
        test_result ret = test_result_pass;
        char out[512];
        memset(out, '\0', 512);
        sprintf(out, "%s/out/%s.out", testDir, testName);
        if (g_file_test(out, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
                xremove(out);
        }
        bnAST* a = bnParseFile(pool, path);
        if (flags & test_mask_expect_pass) {
                bnILToplevel* iltop = bnAST2IL(a);
                writeIL(out, pool, iltop);
                if (a == NULL) {
                        ret = test_result_fail;
                }
                bnDeleteAST(a);
                bnDeleteILTopLevel(iltop);
        } else if (flags & test_mask_expect_fail) {
                if (a != NULL) {
                        ret = test_result_fail;
                }
        }
        bnDeleteStringPool(pool);
        return ret;
}

static test_result test_check_vm(const char* testDir, const char* testName,
                                 const gchar* path, int flags) {
        bnInterpreter* bone = bnNewInterpreter("", bnArgc(), bnArgv());
        test_result ret = test_result_pass;
        char out[512];
        memset(out, '\0', 512);
        sprintf(out, "%s/out/%s.out", testDir, testName);
        if (g_file_test(out, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
                xremove(out);
        }
        // parse and test
        bnAST* a = bnParseFile(bone->pool, path);
        if (flags & test_mask_expect_pass) {
                bnILToplevel* iltop = bnAST2IL(a);
                bnEnviroment* env = bnNewEnviroment(bnIntern(bone->pool, path));
                bnGenerateILTopLevel(bone, iltop, env);
                writeEnv(out, bone->pool, env);
                if (a == NULL) {
                        ret = test_result_fail;
                }
                bnDeleteAST(a);
                bnDeleteILTopLevel(iltop);
                bnDeleteEnviroment(env);
        } else if (flags & test_mask_expect_fail) {
                if (a != NULL) {
                        ret = test_result_fail;
                }
        }
        bnDeleteInterpreter(bone);
        return ret;
}
static test_result test_check_run(const char* testDir, const char* testName,
                                  const gchar* path, int flags) {
        bool panicTest = flags & test_mask_panic;
        test_result res = test_result_pass;
        char out[512];
        memset(out, '\0', 512);
        sprintf(out, "%s/out/%s.std.out", testDir, testName);
        if (g_file_test(out, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
                xremove(out);
        }
#if defined(EXCHANGE_STDOUT)
        FILE* cacheFP = fopen(out, "w");
        if (cacheFP == NULL) {
                perror("test_check_run");
                abort();
        }
        FILE* trueStdout = stdout;
        stdout = cacheFP;
#endif
        bnInterpreter* bone = bnNewInterpreter(path, bnArgc(), bnArgv());
        int ret = bnEval(bone);
#if defined(EXCHANGE_STDOUT)
        stdout = trueStdout;
        fclose(cacheFP);
        writeFile(out);
#endif
        if (flags & test_mask_expect_pass) {
                if (ret != 0 && !panicTest) {
                        res = test_result_fail;
                }
        } else if (flags & test_mask_expect_fail) {
                if (ret == 0) {
                        res = test_result_fail;
                }
        }
        BN_CHECK_MEM();
        bnDeleteInterpreter(bone);
        return res;
}

static test_result test_check(const char* testDir, const char* testName,
                              const gchar* path, int flags) {
        if (flags & test_mask_parse) {
                return test_check_parse(testDir, testName, path, flags);
        } else if (flags & test_mask_vm) {
                return test_check_vm(testDir, testName, path, flags);
        } else if (flags & test_mask_run) {
                return test_check_run(testDir, testName, path, flags);
        } else {
                abort();
        }
        return test_result_unknown;
}

static test_result test_run(const char* testDir, const gchar* path) {
        // testdata/file_type.in
        int pos = bnLastPathComponent(path);
        // file_type.in
        const gchar* filename = path + pos;
        // file_type
        gchar* filename_wext = g_strdup(filename);
        int dotpos = strlen(filename_wext) - 3;
        memset(filename_wext + dotpos, '\0', 3);
        // file type
        char* underbar = strstr(filename_wext, "_");
        gchar* type = NULL;
        if (underbar != NULL) {
                int underbarPos = underbar - filename_wext;
                type = filename_wext + underbarPos + 1;
        }
        int flags = 0;
        // テストの種類を判別する
        if (g_str_has_prefix(type, "Parse")) {
                flags |= test_mask_parse;
                type += 5;
        } else if (g_str_has_prefix(type, "VM")) {
                flags |= test_mask_vm;
                type += 2;
        } else if (g_str_has_prefix(type, "Run")) {
                flags |= test_mask_run;
                type += 3;
        } else {
                printf("no matches to rule: %s\n", path);
                abort();
        }
        //期待するテスト結果を判別
        if (g_str_has_prefix(type, "Pass")) {
                flags |= test_mask_expect_pass;
                type += 4;
        } else if (g_str_has_prefix(type, "Fail")) {
                flags |= test_mask_expect_fail;
                type += 4;
        } else {
                printf("no matches to rule: %s\n", path);
                abort();
        }
        //パニックを期待するかどうか判別する
        if (*type != '\0' && g_str_has_prefix(type, "Panic")) {
                flags |= test_mask_panic;
                type += 5;
        }
        printf("test: %s\n", filename);
        test_result result = test_check(testDir, filename_wext, path, flags);
        g_free(filename_wext);
        return result;
}

int bnTest(const char* dir) {
        int status = 0;
        GPtrArray* fails = g_ptr_array_new_full(2, string_destroy);
        GList* files = get_files(dir);
        GList* iter = files;
        GString* exeDir = bnGetExecutableFileDir();
        while (iter != NULL) {
                gchar* path = iter->data;
                if (!g_str_has_suffix(path, ".in")) {
                        g_free(path);
                        iter->data = NULL;
                        iter = iter->next;
                        continue;
                }
                gchar* fullDir = g_build_filename(exeDir->str, dir, NULL);
                if (test_run(fullDir, path) == test_result_fail) {
                        g_ptr_array_add(fails, g_string_new(path));
                }
                g_free(fullDir);
                g_free(path);
                iter->data = NULL;
                iter = iter->next;
        }
        g_string_free(exeDir, TRUE);
        if (fails->len) {
                printf("failed %d:\n", fails->len);
                for (int i = 0; i < fails->len; i++) {
                        GString* str = g_ptr_array_index(fails, i);
                        printf("    %s\n", str->str);
                }
                status = 1;
        } else {
                printf("Successful completion\n");
        }
        g_list_free(files);
        g_ptr_array_free(fails, TRUE);
        return status;
}
