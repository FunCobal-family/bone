#include "frame.h"
#include "../util/string_pool.h"
#include "array.h"
#include "interpreter.h"
#include "object.h"
#include "snapshot.h"

bnFrame* bnNewFrame() {
        bnFrame* ret = BN_MALLOC(sizeof(bnFrame));
        ret->prev = NULL;
        ret->next = NULL;
        ret->hierarcySelf = bnNewStack();
        ret->vStack = bnNewStack();
        ret->currentCall = NULL;
        ret->depth = 0;
        ret->variableTable =
            g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
        ret->pc = 0;
        ret->panic = NULL;
        ret->snapshots = NULL;
        return ret;
}

bnFrame* bnSubFrame(bnFrame* self) {
        assert(self->next == NULL);
        bnFrame* ret = bnNewFrame();
        self->next = ret;
        ret->prev = self;
        ret->depth = self->depth + 1;
        return ret;
}

bnObject* bnReturnValue(bnFrame* self) {
        if (bnGetStackSize(self->vStack) == 0) {
                return NULL;
        }
        return bnPopStack(self->vStack);
}

void bnInjectFrame(GHashTable* src, bnFrame* dst) {
        GHashTableIter iter;
        gpointer k, v;
        g_hash_table_iter_init(&iter, src);
        while (g_hash_table_iter_next(&iter, &k, &v)) {
                g_hash_table_replace(dst->variableTable, k, v);
        }
}

bnObject* bnExportAllVariable(bnInterpreter* bone, bnFrame* self) {
        bnObject* arr =
            bnNewArray(bone, g_hash_table_size(self->variableTable));
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, self->variableTable);
        gpointer k, v;
        int arrI = 0;
        while (g_hash_table_iter_next(&iter, &k, &v)) {
                bnStringView retName = k;
                const char* retNameStr = bnView2Str(bone->pool, retName);
                if (*retNameStr == '_') {
                        continue;
                }
                bnStringView exportName =
                    bnGetExportVariableName(bone->pool, retName);
                // create private member
                g_hash_table_replace(arr->table, exportName, v);
                bnSetArrayElementAt(arr, arrI, v);
                arrI++;
        }
        return arr;
}

void bnWriteVariable(bnFrame* frame, bnStringView name, bnObject* obj) {
        g_hash_table_replace(frame->variableTable, (gpointer)name, obj);
}

void bnWriteVariable2(bnFrame* frame, struct bnStringPool* pool,
                      const char* name, bnObject* obj) {
        g_hash_table_replace(frame->variableTable,
                             (gpointer)bnIntern(pool, name), obj);
}

void bnDeleteFrame(bnFrame* self) {
        assert(self->next == NULL);
        if (self->prev != NULL) {
                self->prev->next = NULL;
        }
        bnDeleteStack(self->vStack, NULL);
        bnDeleteStack(self->hierarcySelf, NULL);
        g_hash_table_destroy(self->variableTable);
        g_list_free_full(self->snapshots, bnDeleteSnapShot);
        BN_FREE(self);
}