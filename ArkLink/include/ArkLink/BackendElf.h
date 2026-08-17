#ifndef ARKLINK_BACKEND_ELF_H
#define ARKLINK_BACKEND_ELF_H

#include "Context.h"
#include "Backend.h"

#ifdef __cplusplus
extern "C" {
#endif

ArkLinkResult ark_backend_elf_link(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output);

#ifdef __cplusplus
}
#endif

#endif
