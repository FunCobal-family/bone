#include "integer.h"

bnInteger* bnNewInteger(int value) {
        bnInteger* ret = BN_MALLOC(sizeof(bnInteger));
        bnInitObject(&ret->base, BN_OBJECT_INTEGER);
        ret->value = value;
        return ret;
}

void bnDeleteInteger(bnInteger* self) { bnDeleteObject(self); }