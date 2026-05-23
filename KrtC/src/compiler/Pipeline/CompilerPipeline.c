#include <string.h>

extern void* memset(void* s, int c, size_t n);

#include <string.h>
#include "CompilerPipeline.h"
#include "../../Core/Utils/logger.h"
#include "../Frontend/FrontendTemp/FrontendTemp/semantic/Generics.h"
#include "../Frontend/FrontendTemp/FrontendTemp/semantic/NameMangling.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

struct TypeCheckContext;
typedef struct TypeCheckContext TypeCheckContext;
TypeCheckContext* type_check_context_create(void* semantic_analyzer);
void type_check_context_destroy(TypeCheckContext* context);
int type_check_program(TypeCheckContext* context, ASTNode* ast);

static void load_standard_macros(Preprocessor* preprocessor) {
    if (!preprocessor) return;
    
    PreprocessorAddMacro(preprocessor, "println", "Console.WriteLine");
    PreprocessorAddMacro(preprocessor, "println_int", "Console.WriteLineInt");
    PreprocessorAddMacro(preprocessor, "print", "Console.Write");
    PreprocessorAddMacro(preprocessor, "print_int", "Console.WriteInt");
}

static ASTNode* load_single_stdlib_file(const char* file_path);
static ASTNode* merge_ast(ASTNode* main_ast, ASTNode* stdlib_ast);

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static int scan_stdlib_directory(const char* dir_path, char** file_list, int max_files, int* file_count) {
#ifdef _WIN32
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            
            scan_stdlib_directory(full_path, file_list, max_files, file_count);
        } else {
            
            int len = strlen(find_data.cFileName);
            if (len > 4 && strcmp(find_data.cFileName + len - 4, ".krt") == 0) {
                if (*file_count < max_files) {
                    file_list[*file_count] = strdup(full_path);
                    (*file_count)++;
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                
                scan_stdlib_directory(full_path, file_list, max_files, file_count);
            } else {
                
                int len = strlen(entry->d_name);
                if (len > 4 && strcmp(entry->d_name + len - 4, ".krt") == 0) {
                    if (*file_count < max_files) {
                        file_list[*file_count] = strdup(full_path);
                        (*file_count)++;
                    }
                }
            }
        }
    }
    
    closedir(dir);
#endif
    return 1;
}

static ASTNode* load_single_stdlib_file(const char* file_path) {
    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    
    char* source_code = (char*)KRT_MALLOC((size_t)file_size + 1);
    if (!source_code) {
        fclose(fp);
        return NULL;
    }
    
    size_t read_bytes = fread(source_code, 1, (size_t)file_size, fp);
    fclose(fp);
    source_code[read_bytes] = '\0';
    
    Preprocessor* preprocessor = PreprocessorCreate();
    if (!preprocessor) {
        KRT_FREE(source_code);
        return NULL;
    }
    
    load_standard_macros(preprocessor);
    char* processed_source = PreprocessorProcess(preprocessor, source_code);
    PreprocessorDestroy(preprocessor);
    KRT_FREE(source_code);
    
    if (!processed_source) {
        return NULL;
    }
    
    Lexer* lexer = lexer_create(processed_source);
    if (!lexer) {
        KRT_FREE(processed_source);
        return NULL;
    }
    
    Parser* parser = parser_create(lexer);  
    if (!parser) {
        KRT_FREE(processed_source);
        lexer_destroy(lexer);
        return NULL;
    }
    
    ASTNode* stdlib_ast = parser_parse(parser);
    
    KRT_FREE(processed_source);
    lexer_destroy(lexer);
    parser_destroy(parser);
    
    if (!stdlib_ast) {
        return NULL;
    }

    return stdlib_ast;
}

static ASTNode* load_standard_library(const char* stdlib_path) {
    if (!stdlib_path) return NULL;
    
    char* file_list[256];
    int file_count = 0;
    
    if (!scan_stdlib_directory(stdlib_path, file_list, 256, &file_count)) {
        return NULL;
    }

    if (file_count == 0) {
        return NULL;
    }
    
    ASTNode* merged_ast = NULL;
    
    for (int i = 0; i < file_count; i++) {
        ASTNode* file_ast = load_single_stdlib_file(file_list[i]);
        if (file_ast) {
            if (!merged_ast) {
                merged_ast = file_ast;
            } else {
                merged_ast = merge_ast(merged_ast, file_ast);
                
                ast_destroy_node(file_ast);
            }
        }
        free(file_list[i]);
    }
    
    return merged_ast;
}

static ASTNode* merge_ast(ASTNode* main_ast, ASTNode* stdlib_ast) {
    if (!main_ast) return stdlib_ast;
    if (!stdlib_ast) return main_ast;
    
    if (main_ast->type != AST_PROGRAM || stdlib_ast->type != AST_PROGRAM) {
        return main_ast;
    }
    
    int main_count = main_ast->data.block.statement_count;
    int stdlib_count = stdlib_ast->data.block.statement_count;
    int total_count = main_count + stdlib_count;
    
    ASTNode** merged_statements = (ASTNode**)KRT_MALLOC(sizeof(ASTNode*) * (size_t)total_count);
    if (!merged_statements) {
        return main_ast;
    }
    
    for (int i = 0; i < main_count; i++) {
        merged_statements[i] = main_ast->data.block.statements[i];
    }
    
    for (int i = 0; i < stdlib_count; i++) {
        merged_statements[main_count + i] = stdlib_ast->data.block.statements[i];
    }
    
    if (main_ast->data.block.statements) {
        KRT_FREE(main_ast->data.block.statements);
    }
    
    main_ast->data.block.statements = merged_statements;
    main_ast->data.block.statement_count = total_count;
    
    stdlib_ast->data.block.statements = NULL;
    stdlib_ast->data.block.statement_count = 0;
    
    return main_ast;
}

static void record_stage_result(KrtCompilePipeline* pipeline, KrtCompileStage stage, 
                               KrtCompileResult result, double duration, const char* file_name) {
    if (pipeline->stage_count >= 8) return;
    
    KrtCompileStageResult* stage_result = &pipeline->stage_results[pipeline->stage_count++];
    stage_result->stage = stage;
    stage_result->result = result;
    stage_result->duration = duration;
    stage_result->file_name = file_name;
}

KrtCompilePipeline* KrtCompilePipelineCreate(KrtConfig* config, KrtPlatform* platform) {
    KrtCompilePipeline* pipeline = (KrtCompilePipeline*)KRT_MALLOC(sizeof(KrtCompilePipeline));
    if (!pipeline) return NULL;
    
    memset(pipeline, 0, sizeof(KrtCompilePipeline));
    pipeline->config = config;
    pipeline->platform = platform;
    pipeline->success = 1;
    
    name_mangling_init();
    
    return pipeline;
}

void KrtCompilePipelineDestroy(KrtCompilePipeline* pipeline) {
    if (!pipeline) return;
    
    if (pipeline->compiler) {
        KrtCompilerDestroy(pipeline->compiler);
    }
    if (pipeline->type_context) {
        type_check_context_destroy(pipeline->type_context);
    }
    if (pipeline->semantic_analyzer) {
        semantic_analyzer_destroy(pipeline->semantic_analyzer);
        pipeline->semantic_analyzer = NULL;
    }
    if (pipeline->semantic_result) {
        if (pipeline->semantic_result->error_messages) {
            KRT_FREE(pipeline->semantic_result->error_messages);
        }
        KRT_FREE(pipeline->semantic_result);
    }
    if (pipeline->ast) {
        ast_destroy_node(pipeline->ast);
    }
    if (pipeline->parser) {
        parser_destroy(pipeline->parser);
    }
    if (pipeline->lexer) {
        lexer_destroy(pipeline->lexer);
    }
    if (pipeline->processed_source) {
        KRT_FREE(pipeline->processed_source);
    }
    if (pipeline->source_code) {
        KRT_FREE(pipeline->source_code);
    }
    
    name_mangling_cleanup();
    
    KRT_FREE(pipeline);
}

int KrtCompilePipelineReadSource(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->input_file) return 0;
    
    clock_t start = clock();
    
    FILE* fp = fopen(pipeline->input_file, "r");
    if (!fp) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法打开源文件: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_READ_SOURCE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法读取源文件大小: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_READ_SOURCE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    long file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "无法获取源文件大小: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_READ_SOURCE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    rewind(fp);
    pipeline->source_code = (char*)KRT_MALLOC((size_t)file_size + 1);
    if (!pipeline->source_code) {
        fclose(fp);
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "内存分配失败，无法读取源文件: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_READ_SOURCE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    size_t read_bytes = fread(pipeline->source_code, 1, (size_t)file_size, fp);
    fclose(fp);
    pipeline->source_code[read_bytes] = '\0';
    
    record_stage_result(pipeline, KRT_STAGE_READ_SOURCE, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelinePreprocess(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->source_code) return 0;
    
    clock_t start = clock();
    
    Preprocessor* preprocessor = PreprocessorCreate();
    if (!preprocessor) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "预处理器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_PREPROCESS, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    load_standard_macros(preprocessor);
    pipeline->processed_source = PreprocessorProcess(preprocessor, pipeline->source_code);
    PreprocessorDestroy(preprocessor);
    
    if (!pipeline->processed_source) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "预处理失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_PREPROCESS, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, KRT_STAGE_PREPROCESS, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelineLex(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->processed_source) return 0;
    
    clock_t start = clock();
    
    pipeline->lexer = lexer_create(pipeline->processed_source);
    if (!pipeline->lexer) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "词法分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_LEX, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, KRT_STAGE_LEX, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelineParse(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->lexer) return 0;
    
    clock_t start = clock();
    
    pipeline->parser = parser_create(pipeline->lexer);
    if (!pipeline->parser) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语法分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_PARSE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    pipeline->ast = parser_parse(pipeline->parser);
    
    if (!pipeline->ast) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语法分析失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_PARSE, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, KRT_STAGE_PARSE, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelineSemantic(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast) return 0;
    
    clock_t start = clock();
    
    pipeline->semantic_analyzer = semantic_analyzer_create();
    if (!pipeline->semantic_analyzer) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_SEMANTIC, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    pipeline->semantic_result = semantic_analyzer_analyze(pipeline->semantic_analyzer, pipeline->ast);
    if (!pipeline->semantic_result || !pipeline->semantic_result->success) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_SEMANTIC, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    record_stage_result(pipeline, KRT_STAGE_SEMANTIC, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelineTypeCheck(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast) {
        return 0;
    }

    clock_t start = clock();

    pipeline->type_context = type_check_context_create(pipeline->semantic_analyzer);
    if (!pipeline->type_context) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                "类型检查上下文创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_TYPE_CHECK, KRT_RESULT_FAILED,
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }

    if (type_check_program(pipeline->type_context, pipeline->ast) == 0) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message),
                "类型检查失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_TYPE_CHECK, KRT_RESULT_FAILED,
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }

    record_stage_result(pipeline, KRT_STAGE_TYPE_CHECK, KRT_RESULT_SUCCESS,
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

int KrtCompilePipelineCodegen(KrtCompilePipeline* pipeline) {
    if (!pipeline || !pipeline->ast || !pipeline->type_context) return 0;
    
    clock_t start = clock();
    
    KrtTargetPlatform target = KRT_TARGET_X86_ASM;
    if (pipeline->config->target_type == KRT_TARGET_IR) {
        target = KRT_TARGET_IR_TEXT;
    } else if (pipeline->config->target_type == KRT_TARGET_VM) {
        target = KRT_TARGET_VM_BYTECODE;
    } else if (pipeline->config->target_type == KRT_TARGET_KRO) {
        target = KRT_TARGET_KRO_OBJ;
    } else if (pipeline->config->target_type == KRT_TARGET_EXE) {
        target = KRT_TARGET_EXE_PLATFORM;
    }
    
    pipeline->compiler = KrtCompilerCreate(pipeline->output_file, target);
    if (!pipeline->compiler) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "编译器创建失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_CODEGEN, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    if (!pipeline->semantic_result || !pipeline->semantic_result->success) {
        snprintf(pipeline->error_message, sizeof(pipeline->error_message), 
                "语义分析未完成或失败: %s", pipeline->input_file);
        pipeline->success = 0;
        record_stage_result(pipeline, KRT_STAGE_CODEGEN, KRT_RESULT_FAILED, 
                           (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
        return 0;
    }
    
    KrtCompilerCompile(pipeline->compiler, pipeline->ast, pipeline->type_context);
    
    record_stage_result(pipeline, KRT_STAGE_CODEGEN, KRT_RESULT_SUCCESS, 
                       (double)(clock() - start) / CLOCKS_PER_SEC, pipeline->input_file);
    return 1;
}

void KrtCompilePipelineSetMergedAst(KrtCompilePipeline* pipeline, ASTNode* merged_ast) {
    if (!pipeline) return;
    pipeline->ast = merged_ast;
}

int KrtCompilePipelineExecute(KrtCompilePipeline* pipeline, const char* input_file, const char* output_file) {
    if (!pipeline || !input_file || !output_file) return 0;
    
    clock_t total_start = clock();
    
    pipeline->input_file = input_file;
    pipeline->output_file = output_file;
    
    if (pipeline->ast) {
        if (!KrtCompilePipelineSemantic(pipeline)) {
            return 0;
        }
        
        if (!KrtCompilePipelineTypeCheck(pipeline)) {
            return 0;
        }
        
        if (!KrtCompilePipelineCodegen(pipeline)) {
            return 0;
        }
        
        pipeline->total_duration = (double)(clock() - total_start) / CLOCKS_PER_SEC;
        record_stage_result(pipeline, KRT_STAGE_COMPLETE, KRT_RESULT_SUCCESS, pipeline->total_duration, input_file);
        
        return 1;
    }
    
    if (!KrtCompilePipelineReadSource(pipeline)) {
        return 0;
    }
    
    if (!KrtCompilePipelinePreprocess(pipeline)) {
        return 0;
    }
    
    if (!KrtCompilePipelineLex(pipeline)) {
        return 0;
    }
    
    if (!KrtCompilePipelineParse(pipeline)) {
        return 0;
    }
    
    {
        char exe_dir[1024];
#ifdef _WIN32
        GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
        char* last_sep = strrchr(exe_dir, '\\');
        if (last_sep) {
            *last_sep = '\0';
        }
#else
        ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (len != -1) {
            exe_dir[len] = '\0';
            char* last_sep = strrchr(exe_dir, '/');
            if (last_sep) {
                *last_sep = '\0';
            }
        } else {
            strcpy(exe_dir, ".");
        }
#endif
        
        char stdlib_path[1024];
        snprintf(stdlib_path, sizeof(stdlib_path), "%s\\..\\stdlib", exe_dir);
        
        struct stat stdlib_stat;
        if (stat(stdlib_path, &stdlib_stat) == 0 && (stdlib_stat.st_mode & S_IFDIR)) {
            ASTNode* stdlib_ast = load_standard_library(stdlib_path);
            if (stdlib_ast) {
                pipeline->ast = merge_ast(pipeline->ast, stdlib_ast);
                ast_destroy_node(stdlib_ast);
            }
        }
    }
    
    if (!KrtCompilePipelineSemantic(pipeline)) {
        return 0;
    }
    
    if (!KrtCompilePipelineTypeCheck(pipeline)) {
        return 0;
    }
    
    if (!KrtCompilePipelineCodegen(pipeline)) {
        return 0;
    }
    
    pipeline->total_duration = (double)(clock() - total_start) / CLOCKS_PER_SEC;
    record_stage_result(pipeline, KRT_STAGE_COMPLETE, KRT_RESULT_SUCCESS, pipeline->total_duration, input_file);
    
    return 1;
}

int KrtCompilePipelineGetSuccess(KrtCompilePipeline* pipeline) {
    return pipeline ? pipeline->success : 0;
}

const char* KrtCompilePipelineGetError(KrtCompilePipeline* pipeline) {
    return pipeline ? pipeline->error_message : "未知错误";
}

KrtCompileStageResult* KrtCompilePipelineGetStageResults(KrtCompilePipeline* pipeline, int* count) {
    if (!pipeline || !count) return NULL;
    *count = pipeline->stage_count;
    return pipeline->stage_results;
}

double KrtCompilePipelineGetTotalDuration(KrtCompilePipeline* pipeline) {
    return pipeline ? pipeline->total_duration : 0.0;
}