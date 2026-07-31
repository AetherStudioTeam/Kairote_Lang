#ifndef KRT_COMPILER_PIPELINE_H
#define KRT_COMPILER_PIPELINE_H

#include "../../Core/Utils/KrtCommon.h"
#include "../Driver/ConfigManager.h"
#include "../Platform/PlatformAbstraction.h"
#include "../Frontend/Lexer/Tokenizer.h"
#include "../Frontend/Parser/Parser.h"
#include "../Frontend/Semantic/SemanticAnalyzer.h"
#include "../Driver/Preprocessor.h"
#include "../Driver/Compiler.h"

typedef enum {
    KRT_STAGE_READ_SOURCE,
    KRT_STAGE_PREPROCESS,
    KRT_STAGE_LEX,
    KRT_STAGE_PARSE,
    KRT_STAGE_SEMANTIC,
    KRT_STAGE_TYPE_CHECK,
    KRT_STAGE_CODEGEN,
    KRT_STAGE_COMPLETE
} KrtCompileStage;

typedef enum {
    KRT_RESULT_SUCCESS,
    KRT_RESULT_FAILED,
    KRT_RESULT_UP_TO_DATE
} KrtCompileResult;

typedef struct {
    KrtCompileStage stage;
    KrtCompileResult result;
    double duration;
    const char* file_name;
    char error_message[1024];
} KrtCompileStageResult;

typedef struct {
    
    KrtConfig* config;
    KrtPlatform* platform;
    const char* input_file;
    const char* output_file;
    
    KrtCompileStage current_stage;
    char* source_code;
    char* processed_source;
    Lexer* lexer;
    Parser* parser;
    ASTNode* ast;
    SemanticAnalyzer* semantic_analyzer;
    SemanticAnalysisResult* semantic_result;
    void* type_context;
    KrtCompiler* compiler;
    
    KrtCompileStageResult stage_results[8];
    int stage_count;
    double total_duration;
    
    int success;
    char error_message[1024];
} KrtCompilePipeline;

KrtCompilePipeline* KrtCompilePipelineCreate(KrtConfig* config, KrtPlatform* platform);
void KrtCompilePipelineDestroy(KrtCompilePipeline* pipeline);

int KrtCompilePipelineExecute(KrtCompilePipeline* pipeline, const char* input_file, const char* output_file);
void KrtCompilePipelineSetMergedAst(KrtCompilePipeline* pipeline, ASTNode* merged_ast);

int KrtCompilePipelineReadSource(KrtCompilePipeline* pipeline);
int KrtCompilePipelinePreprocess(KrtCompilePipeline* pipeline);
int KrtCompilePipelineLex(KrtCompilePipeline* pipeline);
int KrtCompilePipelineParse(KrtCompilePipeline* pipeline);
int KrtCompilePipelineSemantic(KrtCompilePipeline* pipeline);
int KrtCompilePipelineTypeCheck(KrtCompilePipeline* pipeline);
int KrtCompilePipelineCodegen(KrtCompilePipeline* pipeline);

int KrtCompilePipelineGetSuccess(KrtCompilePipeline* pipeline);
const char* KrtCompilePipelineGetError(KrtCompilePipeline* pipeline);
KrtCompileStageResult* KrtCompilePipelineGetStageResults(KrtCompilePipeline* pipeline, int* count);
double KrtCompilePipelineGetTotalDuration(KrtCompilePipeline* pipeline);

#endif