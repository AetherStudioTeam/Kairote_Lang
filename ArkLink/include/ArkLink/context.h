#ifndef ARKLINK_CONTEXT_H
#define ARKLINK_CONTEXT_H

#include "arklink.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArkSectionBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
    int kind;
    uint32_t flags;
    uint32_t alignment;
} ArkSectionBuffer;

typedef struct ArkLinkContext ArkLinkContext;

ArkLinkContext* ark_context_create(ArkLinkTarget target);
void ark_context_destroy(ArkLinkContext* ctx);

#ifdef __cplusplus
}
#endif

#endif 