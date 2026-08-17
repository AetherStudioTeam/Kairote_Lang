#ifndef KRT_COMPILER_ERROR_H
#define KRT_COMPILER_ERROR_H

#include <stdbool.h>

/* 错误严重程度 */
typedef enum {
    KRT_ERROR_NOTE,      /* 备注信息 */
    KRT_ERROR_WARNING,   /* 警告 */
    KRT_ERROR_ERROR,     /* 错误 */
    KRT_ERROR_FATAL      /* 致命错误 */
} KrtErrorSeverity;

/* 错误阶段 */
typedef enum {
    KRT_ERROR_STAGE_UNKNOWN,
    KRT_ERROR_STAGE_LEX,        /* 词法分析 */
    KRT_ERROR_STAGE_PARSE,      /* 语法分析 */
    KRT_ERROR_STAGE_SEMANTIC,   /* 语义分析 */
    KRT_ERROR_STAGE_TYPE_CHECK, /* 类型检查 */
    KRT_ERROR_STAGE_CODEGEN,    /* 代码生成 */
    KRT_ERROR_STAGE_SSA         /* SSA 构造 */
} KrtErrorStage;

/* 单个错误信息 */
typedef struct {
    KrtErrorSeverity severity;      /* 严重程度 */
    KrtErrorStage stage;            /* 错误阶段 */
    int line;                       /* 起始行号 */
    int column;                     /* 起始列号 */
    int end_line;                   /* 结束行号 */
    int end_column;                 /* 结束列号 */
    char message[512];              /* 错误消息 */
    char hint[256];                 /* 修复建议 */
    char source_line[256];          /* 源代码行 */
    char file_path[256];            /* 文件路径 */
    char error_code[32];            /* 错误代码 */
} KrtCompileError;

/* 错误报告 */
typedef struct {
    KrtCompileError* errors;        /* 错误数组 */
    int error_count;                /* 错误数量 */
    int error_capacity;             /* 数组容量 */
    int warning_count;              /* 警告数量 */
    char* source_code;              /* 源代码内容（用于显示上下文） */
    char file_path[256];            /* 当前编译的文件路径 */
} KrtErrorReport;

/* 创建错误报告 */
KrtErrorReport* KrtErrorReportCreate(void);
void KrtErrorReportDestroy(KrtErrorReport* report);
void KrtErrorReportSetSourceCode(KrtErrorReport* report, const char* source_code);
void KrtErrorReportSetFilePath(KrtErrorReport* report, const char* file_path);
void KrtErrorReportAdd(KrtErrorReport* report, KrtErrorSeverity severity,
                       KrtErrorStage stage, int line, int column,
                       const char* message, const char* hint);
void KrtErrorReportAddEx(KrtErrorReport* report, KrtErrorSeverity severity,
                          KrtErrorStage stage, int line, int column,
                          int end_line, int end_column,
                          const char* error_code, const char* message, 
                          const char* hint);
void KrtErrorReportPrint(KrtErrorReport* report);

/* 获取阶段名称 */
const char* KrtErrorStageName(KrtErrorStage stage);

/* 获取严重程度名称 */
const char* KrtErrorSeverityName(KrtErrorSeverity severity);

#endif /* KRT_COMPILER_ERROR_H */
