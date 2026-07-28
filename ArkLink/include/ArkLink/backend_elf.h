#ifndef ARKLINK_BACKEND_ELF_H
#define ARKLINK_BACKEND_ELF_H

#include "context.h"
#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

ArkLinkResult ark_backend_elf_link(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output);

#ifdef __cplusplus
}
#endif

#endif
