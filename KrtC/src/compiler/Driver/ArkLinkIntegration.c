#include "ArkLinkIntegration.h"
#include "../../Core/Utils/KrtCommon.h"
#include "../../Core/Utils/logger.h"
#include "../../Core/Utils/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

struct KrtArkLinkContext {
    ArkLinkSession* session;
    KrtConfig* config;
    char** obj_files;
    int obj_count;
    int obj_capacity;
};

static void ArkLinkLoggerCallback(ArkLogLevel level, const char* message, void* user_data) {
    (void)user_data;
    (void)level;
    (void)message;
}

KrtArkLinkContext* KrtArkLinkContextCreate(KrtConfig* config) {
    if (!config) return NULL;
    
    KrtArkLinkContext* ctx = (KrtArkLinkContext*)KRT_MALLOC(sizeof(KrtArkLinkContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(KrtArkLinkContext));
    ctx->config = config;
    ctx->obj_capacity = 64;
    ctx->obj_files = (char**)KRT_MALLOC(ctx->obj_capacity * sizeof(char*));
    
    if (!ctx->obj_files) {
        free(ctx);
        return NULL;
    }
    
    ctx->session = arklink_session_create();
    if (!ctx->session) {
        free(ctx->obj_files);
        free(ctx);
        return NULL;
    }
    
    arklink_session_set_logger(ctx->session, ArkLinkLoggerCallback, ctx);
    
    ArkLinkTarget target = (config->platform == KRT_CONFIG_PLATFORM_WINDOWS) 
                          ? ARK_LINK_TARGET_PE 
                          : ARK_LINK_TARGET_ELF;
    arklink_session_set_target(ctx->session, target);
    
    arklink_session_set_output_kind(ctx->session, ARK_LINK_OUTPUT_EXECUTABLE);
    
    const KrtLinkerConfig* linker_config = KrtConfigGetLinkerConfig(config->platform);
    if (linker_config && linker_config->entry_point) {
        arklink_session_set_entry_point(ctx->session, linker_config->entry_point);
    } else {
        
        arklink_session_set_entry_point(ctx->session, "_ZN4mainEv");
    }
    
    if (config->platform == KRT_CONFIG_PLATFORM_WINDOWS) {
        arklink_session_set_subsystem(ctx->session, ARK_SUBSYSTEM_CONSOLE);
        arklink_session_set_image_base(ctx->session, 0x140000000ULL);
    } else {
        arklink_session_set_image_base(ctx->session, 0x400000ULL);
    }
    
    arklink_session_set_stack_size(ctx->session, 0x100000);
    
    return ctx;
}

void KrtArkLinkContextDestroy(KrtArkLinkContext* ctx) {
    if (!ctx) return;
    
    if (ctx->session) {
        arklink_session_destroy(ctx->session);
    }
    
    if (ctx->obj_files) {
        for (int i = 0; i < ctx->obj_count; i++) {
            free(ctx->obj_files[i]);
        }
        free(ctx->obj_files);
    }
    
    free(ctx);
}

int KrtArkLinkAddObjectFile(KrtArkLinkContext* ctx, const char* obj_path) {
    if (!ctx || !obj_path) return -1;
    
    if (ctx->obj_count >= ctx->obj_capacity) {
        int new_capacity = ctx->obj_capacity * 2;
        char** new_files = (char**)KRT_REALLOC(ctx->obj_files, new_capacity * sizeof(char*));
        if (!new_files) return -1;
        ctx->obj_files = new_files;
        ctx->obj_capacity = new_capacity;
    }
    
    ctx->obj_files[ctx->obj_count] = KRT_STRDUP(obj_path);
    if (!ctx->obj_files[ctx->obj_count]) return -1;
    
    ArkLinkResult result = arklink_session_add_input(ctx->session, obj_path);
    if (result != ARK_LINK_OK) {
        KrtError("Failed to add object file: %s", obj_path);
        return -1;
    }
    
    ctx->obj_count++;
    return 0;
}

int KrtArkLinkAddRuntimeObjects(KrtArkLinkContext* ctx, const char* runtime_dir) {
    if (!ctx || !runtime_dir) return -1;
    
    const char* runtime_files[] = {
        "runtime.o",
        "output_cache.o", 
        "allocator.o",
        "KrtString.o",
        NULL
    };
    
    for (int i = 0; runtime_files[i]; i++) {
        char path[KRT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%c%s", 
                 runtime_dir, KRT_PATH_SEPARATOR, runtime_files[i]);
        
        if (KrtPathExists(path)) {
            KrtArkLinkAddObjectFile(ctx, path);
        }
    }
    
    return 0;
}

int KrtArkLinkSetOutput(KrtArkLinkContext* ctx, const char* output_path) {
    if (!ctx || !output_path) return -1;
    
    ArkLinkResult result = arklink_session_set_output(ctx->session, output_path);
    if (result != ARK_LINK_OK) {
        KrtError("Failed to set output path: %s", output_path);
        return -1;
    }
    
    return 0;
}

int KrtArkLinkSetEntryPoint(KrtArkLinkContext* ctx, const char* entry_point) {
    if (!ctx || !entry_point) return -1;
    
    ArkLinkResult result = arklink_session_set_entry_point(ctx->session, entry_point);
    if (result != ARK_LINK_OK) {
        KrtError("Failed to set entry point: %s", entry_point);
        return -1;
    }
    
    return 0;
}

int KrtArkLinkLink(KrtArkLinkContext* ctx) {
    if (!ctx || !ctx->session) return -1;

    ArkLinkResult result = arklink_session_link(ctx->session);

    if (result != ARK_LINK_OK) {
        const char* error = arklink_session_get_error(ctx->session);
        KrtError("ArkLink failed: %s", error ? error : "Unknown error");
        return -1;
    }

    return 0;
}

int KrtArkLinkLinkObjects(const char** obj_files, int obj_count, 
                            const char* output_path, KrtConfig* config) {
    if (!obj_files || obj_count <= 0 || !output_path || !config) {
        return -1;
    }
    
    KrtArkLinkContext* ctx = KrtArkLinkContextCreate(config);
    if (!ctx) {
        KrtError("Failed to create ArkLink context");
        return -1;
    }
    
    for (int i = 0; i < obj_count; i++) {
        KrtArkLinkAddObjectFile(ctx, obj_files[i]);
    }
    
    if (KrtArkLinkSetOutput(ctx, output_path) != 0) {
        KrtError("Failed to set output path");
        KrtArkLinkContextDestroy(ctx);
        return -1;
    }
    
    int result = KrtArkLinkLink(ctx);
    
    KrtArkLinkContextDestroy(ctx);
    
    return result;
}