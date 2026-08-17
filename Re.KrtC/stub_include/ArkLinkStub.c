// Stub implementations for ArkLink functions
// These provide minimal functionality for compilation purposes

#include "ArkLink/Arklink.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct ArkLinkSession {
    ArkLinkTarget target;
    ArkLinkOutputKind output_kind;
    char* output_path;
    char* entry_point;
    ArkLinkSubsystem subsystem;
    unsigned long long image_base;
    unsigned long long stack_size;
    ArkLinkLogger logger;
    void* logger_data;
    char* error_message;
};

static ArkLinkSession* default_session = NULL;

ArkLinkSession* arklink_session_create(void) {
    ArkLinkSession* session = (ArkLinkSession*)calloc(1, sizeof(ArkLinkSession));
    if (session) {
        session->target = ARK_LINK_TARGET_ELF;
        session->output_kind = ARK_LINK_OUTPUT_EXECUTABLE;
        session->image_base = 0x400000ULL;
        session->stack_size = 0x100000ULL;
    }
    return session;
}

void arklink_session_destroy(ArkLinkSession* session) {
    if (session) {
        free(session->output_path);
        free(session->entry_point);
        free(session->error_message);
        free(session);
    }
}

ArkLinkResult arklink_session_set_logger(ArkLinkSession* session, ArkLinkLogger logger, void* user_data) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->logger = logger;
    session->logger_data = user_data;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_target(ArkLinkSession* session, ArkLinkTarget target) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->target = target;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_output_kind(ArkLinkSession* session, ArkLinkOutputKind kind) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->output_kind = kind;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_output(ArkLinkSession* session, const char* path) {
    if (!session || !path) return ARK_LINK_INVALID_INPUT;
    free(session->output_path);
    session->output_path = strdup(path);
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_entry_point(ArkLinkSession* session, const char* entry) {
    if (!session || !entry) return ARK_LINK_INVALID_INPUT;
    free(session->entry_point);
    session->entry_point = strdup(entry);
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_subsystem(ArkLinkSession* session, ArkLinkSubsystem subsystem) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->subsystem = subsystem;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_image_base(ArkLinkSession* session, unsigned long long base) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->image_base = base;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_set_stack_size(ArkLinkSession* session, unsigned long long size) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    session->stack_size = size;
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_add_input(ArkLinkSession* session, const char* path) {
    if (!session || !path) return ARK_LINK_INVALID_INPUT;
    // Stub: just return OK
    return ARK_LINK_OK;
}

ArkLinkResult arklink_session_link(ArkLinkSession* session) {
    if (!session) return ARK_LINK_INVALID_INPUT;
    // Stub: print a message and return OK
    fprintf(stderr, "[ArkLink Stub] Linking would happen here (output: %s)\n",
            session->output_path ? session->output_path : "unknown");
    return ARK_LINK_OK;
}

const char* arklink_session_get_error(ArkLinkSession* session) {
    if (!session) return "Invalid session";
    return session->error_message ? session->error_message : "Unknown error";
}
