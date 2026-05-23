#include "BuildSystem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "ArkLink/arklink.h"
#include "ArkLink/loader.h"

extern void FindRuntimeObj(const char* obj_name, char* result, size_t size);

KrtBuildContext* KrtBuildContextCreate(KrtConfig* config, KrtPlatform* platform) {
    KrtBuildContext* ctx = (KrtBuildContext*)malloc(sizeof(KrtBuildContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(KrtBuildContext));
    ctx->config = config;
    ctx->platform = platform;
    ctx->temp_asm_created = 0;
    ctx->error_message[0] = '\0';
    
    return ctx;
}

void KrtBuildContextDestroy(KrtBuildContext* ctx) {
    if (!ctx) return;
    
    if (ctx->temp_asm_created && ctx->temp_asm_file[0]) {
        remove(ctx->temp_asm_file);
    }
    
    free(ctx);
}

static int run_command(const char* cmd, char* output, size_t output_size) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    if (!pipe) return 0;
    
    if (output && output_size > 0) {
        output[0] = '\0';
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            strncat(output, buffer, output_size - strlen(output) - 1);
        }
    }
    
#ifdef _WIN32
    int status = _pclose(pipe);
    return status == 0;
#else
    int status = pclose(pipe);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static int link_with_arklink(const char* input_file, const char* output_file, char* error_msg, size_t error_size) {
    ArkLinkResult result;
    
    ArkLinkSession* session = arklink_session_create();
    if (!session) {
        strncpy(error_msg, "Failed to create ArkLink session", error_size - 1);
        return 0;
    }
    
    result = arklink_session_set_target(session, ARK_LINK_TARGET_PE);
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Failed to set target: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_output(session, output_file);
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Failed to set output: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_output_kind(session, ARK_LINK_OUTPUT_EXECUTABLE);
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Failed to set output kind: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_entry_point(session, "main");
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Failed to set entry point: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_add_input(session, input_file);
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Failed to add input: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    char runtime_obj[KRT_MAX_PATH];
    char cache_obj[KRT_MAX_PATH];
    char allocator_obj[KRT_MAX_PATH];
    char KrtStringObj[KRT_MAX_PATH];
    
    FindRuntimeObj("runtime.o", runtime_obj, sizeof(runtime_obj));
    FindRuntimeObj("output_cache.o", cache_obj, sizeof(cache_obj));
    FindRuntimeObj("allocator.o", allocator_obj, sizeof(allocator_obj));
    FindRuntimeObj("KrtString.o", KrtStringObj, sizeof(KrtStringObj));
    
    if (runtime_obj[0]) {
        result = arklink_session_add_input(session, runtime_obj);
        if (result != ARK_LINK_OK) {
            snprintf(error_msg, error_size, "Failed to add runtime.o: %d", result);
            arklink_session_destroy(session);
            return 0;
        }
    }
    if (cache_obj[0]) {
        result = arklink_session_add_input(session, cache_obj);
        if (result != ARK_LINK_OK) {
            snprintf(error_msg, error_size, "Failed to add output_cache.o: %d", result);
            arklink_session_destroy(session);
            return 0;
        }
    }
    if (allocator_obj[0]) {
        result = arklink_session_add_input(session, allocator_obj);
        if (result != ARK_LINK_OK) {
            snprintf(error_msg, error_size, "Failed to add allocator.o: %d", result);
            arklink_session_destroy(session);
            return 0;
        }
    }
    if (KrtStringObj[0]) {
        result = arklink_session_add_input(session, KrtStringObj);
        if (result != ARK_LINK_OK) {
            snprintf(error_msg, error_size, "Failed to add KrtString.o: %d", result);
            arklink_session_destroy(session);
            return 0;
        }
    }
    
    result = arklink_session_link(session);
    if (result != ARK_LINK_OK) {
        snprintf(error_msg, error_size, "Linking failed: %d", result);
        arklink_session_destroy(session);
        return 0;
    }
    
    arklink_session_destroy(session);
    return 1;
}

int KrtBuildExecute(KrtBuildContext* ctx, const char* input_file, const char* output_file) {
    if (!ctx) {
        return 0;
    }
    
    const char* ext = strrchr(input_file, '.');
    if (ext && (strcmp(ext, ".eo") == 0 || strcmp(ext, ".krt") == 0)) {
        if (!link_with_arklink(input_file, output_file, ctx->error_message, sizeof(ctx->error_message))) {
            return 0;
        }
        return 1;
    }
    
    if (!ctx->temp_asm_file[0]) {
        strncpy(ctx->error_message, "No assembly file to build", sizeof(ctx->error_message));
        return 0;
    }
    
    ArkLinkSession* session = arklink_session_create();
    if (!session) {
        strncpy(ctx->error_message, "Failed to create ArkLink session", sizeof(ctx->error_message));
        return 0;
    }
    
    ArkLinkResult result = arklink_session_set_target(session, ARK_LINK_TARGET_PE);
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to set target: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_output(session, output_file);
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to set output: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_output_kind(session, ARK_LINK_OUTPUT_EXECUTABLE);
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to set output kind: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_set_entry_point(session, "main");
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to set entry point: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    result = arklink_session_add_input(session, ctx->temp_asm_file);
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to add input: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    char runtime_lib_path[KRT_MAX_PATH];
    const char* env_lib = getenv("KRO_RUNTIME_LIB");
    if (env_lib) {
        strncpy(runtime_lib_path, env_lib, sizeof(runtime_lib_path) - 1);
    } else {
        
        char output_dir[KRT_MAX_PATH];
        strncpy(output_dir, output_file, sizeof(output_dir) - 1);
        char* last_slash = strrchr(output_dir, '/');
        if (!last_slash) last_slash = strrchr(output_dir, '\\');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(runtime_lib_path, sizeof(runtime_lib_path), "%s/bin/runtime_console.o", output_dir);
        } else {
            snprintf(runtime_lib_path, sizeof(runtime_lib_path), "bin/runtime_console.o");
        }
    }
    
    FILE* f = fopen(runtime_lib_path, "rb");
    if (!f) {
        snprintf(runtime_lib_path, sizeof(runtime_lib_path), "runtime_console.o");
        f = fopen(runtime_lib_path, "rb");
    }
    if (f) {
        fclose(f);
        result = arklink_session_add_input(session, runtime_lib_path);
        if (result != ARK_LINK_OK) {
            snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to add runtime: %s", arklink_session_get_error(session));
            arklink_session_destroy(session);
            return 0;
        }
    }
    
    result = arklink_session_link(session);
    if (result != ARK_LINK_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message), "Linking failed: %s", arklink_session_get_error(session));
        arklink_session_destroy(session);
        return 0;
    }
    
    arklink_session_destroy(session);
    return 1;
}

const char* KrtBuildGetError(KrtBuildContext* ctx) {
    if (!ctx) return "Unknown error";
    if (ctx->error_message[0]) {
        return ctx->error_message;
    }
    return "Build failed";
}