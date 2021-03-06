﻿#include "array.h"
#include <assert.h>
#include "char.h"
#include "frame.h"
#include "heap.h"
#include "integer.h"
#include "interpreter.h"
#include "keyword.h"
#include "lambda.h"
#include "storage.h"

#define _throw(bone, frame, fmt) (bnFormatThrow(bone, fmt))

static void bnStdArrayArraySet(bnInterpreter* bone, bnFrame* frame);
static void bnStdArrayArrayGet(bnInterpreter* bone, bnFrame* frame);
static void free_array(bnStorage* storage, bnReference ref, bnObject* obj);
/**
 * bnArray is array.
 */
typedef struct bnArray {
        bnObject base;
        GPtrArray* arr;
        int size;
} bnArray;

bnReference bnNewArray(bnInterpreter* bone, int size) {
        bnReference ref = bnAllocObject(bone->heap);
        bnArray* ret = bnGetObject(bone->heap, ref);
        bnInitObject(bone, &ret->base, BN_OBJECT_ARRAY);
        ret->base.freeFunc = free_array;
        ret->arr = g_ptr_array_new();
        ret->size = size;
        g_ptr_array_set_free_func(ret->arr, NULL);
        for (int i = 0; i < size; i++) {
                ret = bnGetObject(bone->heap, ref);
                assert(ret->arr != NULL);
                g_ptr_array_add(ret->arr, bnNewObject(bone));
        }
        ret = bnGetObject(bone->heap, ref);
        bnDefine(&ret->base, bnIntern(bone->pool, "length"),
                 bnNewInteger(bone, size));
        ret = bnGetObject(bone->heap, ref);
        bnDefine(&ret->base, bnIntern(bone->pool, BN_KWD_ARRAY_SET),
                 bnNewLambdaFromCFunc(bone, bnStdArrayArraySet, bone->pool,
                                      BN_C_ADD_PARAM, "self", BN_C_ADD_PARAM,
                                      "index", BN_C_ADD_PARAM, "value",
                                      BN_C_ADD_EXIT));
        ret = bnGetObject(bone->heap, ref);
        bnDefine(&ret->base, bnIntern(bone->pool, BN_KWD_ARRAY_GET),
                 bnNewLambdaFromCFunc(bone, bnStdArrayArrayGet, bone->pool,
                                      BN_C_ADD_PARAM, "self", BN_C_ADD_PARAM,
                                      "index", BN_C_ADD_RETURN, "ret",
                                      BN_C_ADD_EXIT));
        ret = bnGetObject(bone->heap, ref);
        bnDefine(&ret->base, bnIntern(bone->pool, "set"),
                 bnNewLambdaFromCFunc(bone, bnStdArrayArraySet, bone->pool,
                                      BN_C_ADD_PARAM, "self", BN_C_ADD_PARAM,
                                      "index", BN_C_ADD_PARAM, "value",
                                      BN_C_ADD_RETURN, "ret", BN_C_ADD_EXIT));
        ret = bnGetObject(bone->heap, ref);
        bnDefine(&ret->base, bnIntern(bone->pool, "get"),
                 bnNewLambdaFromCFunc(bone, bnStdArrayArrayGet, bone->pool,
                                      BN_C_ADD_PARAM, "self", BN_C_ADD_PARAM,
                                      "index", BN_C_ADD_RETURN, "ret",
                                      BN_C_ADD_EXIT));
        return ref;
}

void bnFillString(bnInterpreter* bone, const char* str, bnReference ref) {
        bnArray* ary = (bnArray*)bnGetObject(bone->heap, ref);
        for (int i = 0; i < ary->size; i++) {
                g_ptr_array_index(ary->arr, i) = bnNewChar(bone, str[i]);
        }
}

int bnGetArrayLength(bnObject* obj) { return ((bnArray*)obj)->arr->len; }

bnReference bnGetArrayElementAt(bnObject* obj, int index) {
        return (bnReference)g_ptr_array_index(((bnArray*)obj)->arr, index);
}

void bnSetArrayElementAt(bnObject* obj, int index, bnReference ref) {
        g_ptr_array_index(((bnArray*)obj)->arr, index) = ref;
}

// private
static void bnStdArrayArraySet(bnInterpreter* bone, bnFrame* frame) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
        // self:Array
        // index:Integer
        bnPopArrayArg(bone, frame, self);
        bnPopIntArg(bone, frame, index);
        bnPopArg(bone, frame, val);
        bnSetArrayElementAt(selfObj, bnGetIntegerValue(indexObj), valRef);
#pragma clang diagnostic pop
}

static void bnStdArrayArrayGet(bnInterpreter* bone, bnFrame* frame) {
        // self:Array
        // index:Integer
        bnPopArrayArg(bone, frame, self);
        bnPopIntArg(bone, frame, index);
        bnWriteVariable2(
            frame, bone->pool, "ret",
            bnGetArrayElementAt(selfObj, bnGetIntegerValue(indexObj)));
}

static void free_array(bnStorage* storage, bnReference ref, bnObject* obj) {
        obj->freeFunc = NULL;
        bnArray* arr = (bnArray*)obj;
        g_ptr_array_unref(arr->arr);
        bnDeleteObject(storage, ref, obj);
}