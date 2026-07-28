#include <string.h>
extern char* strcpy(char* dest, const char* src);
extern char* strncpy(char* dest, const char* src, size_t n);
extern char* strrchr(const char* str, int c);
extern int strcmp(const char* s1, const char* s2);
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include "Core/Core.h"
#include "compiler/Driver/Compiler.h"
#include "compiler/compiler.h"
#include "compiler/Driver/ConfigManager.h"
#include "compiler/Platform/PlatformAbstraction.h"
#include "compiler/Build/BuildSystem.h"
#include "compiler/Pipeline/CompilerPipeline.h"
#include "compiler/Driver/TaskManager.h"
#include "compiler/Driver/ConsoleUtils.h"
#include "Version.h"
struct TypeCheckContext;
typedef struct TypeCheckContext TypeCheckContext;
TypeCheckContext* type_check_context_create(void* semantic_analyzer);
void type_check_context_destroy(TypeCheckContext* context);
int type_check_program(TypeCheckContext* context, ASTNode* ast);
#include "compiler/Frontend/FrontendTemp/FrontendTemp/parser/Parser.h"
#include "compiler/Frontend/FrontendTemp/FrontendTemp/lexer/Tokenizer.h"
#include "compiler/Driver/Project.h"
#include "compiler/Driver/ParallelCompiler.h"
#include "compiler/Frontend/FrontendTemp/FrontendTemp/semantic/SemanticAnalyzer.h"
#include "compiler/Driver/Preprocessor.h"
#include "compiler/Frontend/FrontendTemp/FrontendTemp/semantic/Generics.h"
#include "compiler/Driver/ArkLinkIntegration.h"
extern void KrtOutputCacheInit(void);
extern void KrtOutputCacheCleanup(void);
extern void KrtOutputCacheSetEnabled(int enabled);
extern void KrtOutputCacheFlush(void);

static const char* project_name_from_type(const char* type) {
    if (!type) return "console";
    if (strcmp(type, "console") == 0) return "console";
    if (strcmp(type, "lib") == 0) return "library";
    if (strcmp(type, "library") == 0) return "library";
    if (strcmp(type, "web") == 0) return "web";
    return "console";
}

static void KrtCreateProject(const char* name, const char* type) {
    (void)type;
    KrtProject* project = KrtProjCreate(name, KRT_PROJ_TYPE_CONSOLE);
    if (project) {
        KrtProjCreateTemplate(project, name);
        KrtProjDestroy(project);
    } else {
        KrtError("项目创建失败!");
    }
}

typedef enum {
    KRT_TARGET_CMD_ASM = 0,
    KRT_TARGET_CMD_IR = 3,
    KRT_TARGET_CMD_EXE = 4,
    KRT_TARGET_CMD_VM = 5,
    KRT_TARGET_CMD_BUILD = 6,
    KRT_TARGET_CMD_CLEAN = 7,
    KRT_TARGET_CMD_CHECK = 8,
    KRT_TARGET_CMD_VERSION = 9,
    KRT_TARGET_CMD_KRO = 10    
} KrtCommandTargetType;

#define KRT_MAX_INPUT_FILES 32

typedef struct {
    const char* input_file;
    const char* input_files[KRT_MAX_INPUT_FILES];
    int input_file_count;
    const char* output_file;
    KrtCommandTargetType target_type;
    int show_ir;
    int show_help;
    int create_project;
    int keep_temp_files;
    const char* project_type;
    int output_file_set;
    int target_type_set;
} KrtCommandLineOptions;

static void KrtPrintUsage(const char* program_name) {
    (void)program_name;
    const char* blue = KrtColor(KRT_COL_BLUE);
    const char* gray = KrtColor(KRT_COL_GRAY);
    const char* reset = KrtColor(KRT_COL_RESET);
    
    KrtPrintf("%s||Kairote Lang 使用说明%s\n", blue, reset);
    KrtPrintf("%sbuild%s:%s构建%s  clean%s:%s清理\n", blue, gray, blue, blue, gray, blue);
    KrtPrintf("%scheck%s:%s检查%s  help%s:%s帮助信息\n", blue, gray, blue, blue, gray, blue);
    KrtPrintf("%s--keep-temp%s:%s保留临时文件 (.kro, .eo)\n", blue, gray, blue);
    KrtPrintf("\n%s=========== %s其他 %s===========\n", gray, gray);
    KrtPrintf("%starget%s:%s输出类型 %s<%sir%s/%sasm%s/%sexe%s/%svm%s/%seo%s>\n", 
              blue, gray, blue, gray, blue, gray, blue, gray, blue, gray, blue, gray, blue, gray);
    KrtPrintf("%snew%s:%s创建项目 %s<%s类型%s> <%s项目名%s>\n", blue, gray, blue, gray, blue, gray, blue, gray);
    KrtPrintf("%sversion%s:%s %s\n", blue, gray, blue, KRT_COMPILER_VERSION);
    KrtPrintf("%s", reset);
}

static KrtCommandLineOptions KrtParseCommandLine(int argc, char* argv[]) {
    KrtCommandLineOptions options;
    memset(&options, 0, sizeof(options));
    options.target_type = KRT_TARGET_CMD_EXE;

    if (argc < 2) {
        options.show_help = 1;
        return options;
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        
        if (strcmp(arg, "help") == 0) {
            options.show_help = 1;
        } else if (strcmp(arg, "--keep-temp") == 0) {
            options.keep_temp_files = 1;
        } else if (strcmp(arg, "new") == 0) {
            if (i + 2 >= argc) {
                KrtError("缺少参数: new <类型> <项目名>");
                exit(1);
            }
            options.create_project = 1;
            options.project_type = argv[++i];
            options.input_file = argv[++i];
        } else if (strcmp(arg, "build") == 0) {
            if (i + 1 < argc) {
                options.target_type = KRT_TARGET_CMD_BUILD;
                options.input_file = argv[++i];
            } else {
                options.target_type = KRT_TARGET_CMD_BUILD;
                
                #ifdef _WIN32
                WIN32_FIND_DATA findData;
                HANDLE hFind = FindFirstFile("*.esproj", &findData);
                if (hFind != INVALID_HANDLE_VALUE) {
                    options.input_file = strdup(findData.cFileName);
                    FindClose(hFind);
                } else {
                    KrtError("未找到 .esproj 文件，请指定输入文件");
                    exit(1);
                }
                #else
                DIR *dir = opendir(".");
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        const char* ext = strrchr(entry->d_name, '.');
                        if (ext && strcmp(ext, ".esproj") == 0) {
                            options.input_file = strdup(entry->d_name);
                            break;
                        }
                    }
                    closedir(dir);
                }
                if (!options.input_file) {
                    KrtError("未找到 .esproj 文件，请指定输入文件");
                    exit(1);
                }
                #endif
            }
        } else if (strcmp(arg, "clean") == 0) {
            options.target_type = KRT_TARGET_CMD_CLEAN;
        } else if (strcmp(arg, "check") == 0) {
            options.target_type = KRT_TARGET_CMD_CHECK;
        } else if (strcmp(arg, "target") == 0) {
            if (i + 1 >= argc) {
                KrtError("缺少参数: target <类型>");
                exit(1);
            }
            options.target_type_set = 1;
            const char* target_type = argv[++i];
            if (strcmp(target_type, "asm") == 0) {
                options.target_type = KRT_TARGET_CMD_ASM;
            } else if (strcmp(target_type, "exe") == 0) {
                options.target_type = KRT_TARGET_CMD_EXE;
            } else if (strcmp(target_type, "ir") == 0) {
                options.target_type = KRT_TARGET_CMD_IR;
            } else if (strcmp(target_type, "vm") == 0) {
                options.target_type = KRT_TARGET_CMD_VM;
            } else if (strcmp(target_type, "eo") == 0) {
                options.target_type = KRT_TARGET_CMD_KRO;
            } else {
                KrtError("未知的目标类型 '%s'", target_type);
                exit(1);
            }
        } else if (strcmp(arg, "version") == 0) {
            options.target_type = KRT_TARGET_CMD_VERSION;
        } else if (strcmp(arg, "output") == 0) {
            if (i + 1 >= argc) {
                KrtError("缺少参数: output <输出文件>");
                exit(1);
            }
            options.output_file = argv[++i];
            options.output_file_set = 1;
        } else if (strcmp(arg, "--show-ir") == 0 || strcmp(arg, "--ir") == 0) {
            options.show_ir = 1;
         } else if (options.target_type != KRT_TARGET_CMD_CLEAN &&
                   options.target_type != KRT_TARGET_CMD_CHECK && options.target_type != KRT_TARGET_CMD_VERSION) {
            
            if (options.input_file_count < KRT_MAX_INPUT_FILES) {
                options.input_files[options.input_file_count++] = arg;
                if (options.input_file == NULL) {
                    options.input_file = arg; 
                }
            } else {
                KrtError("Too many input files (max %d)", KRT_MAX_INPUT_FILES);
                exit(1);
            }
        } else {
            KrtError("未知命令或参数: %s", arg);
            options.show_help = 1;
        }
    }

    return options;
}

static void KrtGetExecutableDir(char* buffer, int buffer_size);
static int KrtScanStdlibDir(const char* dir_path, char** file_list, int max_files, int* file_count);
static void KrtGetKroFilename(const char* krt_file, char* kro_file, int kro_file_size);
static int KrtNeedsRecompile(const char* krt_file, const char* kro_file);
static int KrtCompileStdlib(KrtConfig* config, KrtPlatform* platform,
                             char** stdlib_krt_files, int stdlib_count,
                             char** stdlib_kro_files, int max_kro_files);

static int KrtLinkKroExecutable(KrtConfig* config, KrtPlatform* platform,
                                const char* kro_file, const char* exe_output) {
    if (!KrtPlatformPathExists(platform, kro_file)) {
        KrtError("KRO 文件不存在: %s", kro_file);
        return 0;
    }

    char exe_dir[KRT_MAX_PATH];
    KrtGetExecutableDir(exe_dir, sizeof(exe_dir));

    char stdlib_dir[KRT_MAX_PATH];
    char exe_dir_truncated[1010];
    strncpy(exe_dir_truncated, exe_dir, sizeof(exe_dir_truncated) - 1);
    exe_dir_truncated[sizeof(exe_dir_truncated) - 1] = '\0';
    snprintf(stdlib_dir, sizeof(stdlib_dir), "%s" KRT_PATH_SEPARATOR_STR ".." KRT_PATH_SEPARATOR_STR "stdlib", exe_dir_truncated);

    char** stdlib_krt_files = NULL;
    char** stdlib_kro_files = NULL;
    int stdlib_count = 0;
    int stdlib_kro_count = 0;
    struct stat stdlib_stat;

    if (stat(stdlib_dir, &stdlib_stat) == 0 && (stdlib_stat.st_mode & S_IFDIR)) {
        stdlib_krt_files = (char**)malloc(256 * sizeof(char*));
        stdlib_kro_files = (char**)malloc(256 * sizeof(char*));

        if (stdlib_krt_files && stdlib_kro_files) {
            KrtScanStdlibDir(stdlib_dir, stdlib_krt_files, 256, &stdlib_count);

            if (stdlib_count > 0) {
                stdlib_kro_count = KrtCompileStdlib(config, platform,
                                                     stdlib_krt_files, stdlib_count,
                                                     stdlib_kro_files, 256);
            }
        }
    }

    int total_obj_count = 1 + stdlib_kro_count;
    const char** obj_files = (const char**)malloc(total_obj_count * sizeof(const char*));
    int result = 0;
    if (!obj_files) {
        KrtError("Failed to allocate memory for object files");
    } else {
        int obj_count = 0;
        obj_files[obj_count++] = kro_file;

        for (int i = 0; i < stdlib_kro_count; i++) {
            if (stdlib_kro_files[i] && KrtPlatformPathExists(platform, stdlib_kro_files[i])) {
                obj_files[obj_count++] = stdlib_kro_files[i];
            }
        }

        if (KrtArkLinkLinkObjects(obj_files, obj_count, exe_output, config) == 0) {
            KrtTaskReport("link", kro_file, KRT_TASK_RESULT_EXECUTED, 0.0, KrtGetGlobalTaskStats());
            result = 1;
        } else {
            KrtError("ArkLink 链接失败");
            KrtTaskReport("link", kro_file, KRT_TASK_RESULT_FAILED, 0.0, KrtGetGlobalTaskStats());
            result = 0;
        }

        free(obj_files);
    }

    if (stdlib_krt_files) {
        for (int i = 0; i < stdlib_count; i++) {
            if (stdlib_krt_files[i]) free(stdlib_krt_files[i]);
        }
        free(stdlib_krt_files);
    }
    if (stdlib_kro_files) {
        for (int i = 0; i < stdlib_kro_count; i++) {
            if (stdlib_kro_files[i]) {
                free(stdlib_kro_files[i]);
            }
        }
        free(stdlib_kro_files);
    }

    return result;
}

static int KrtCompileSingleFile(const char* input_file, const char* output_file, KrtCommandTargetType target_type, int show_ir, int keep_temp) {
    if (!input_file) {
        KrtError("输入文件为空");
        return 1;
    }
    
    char default_output[KRT_MAX_PATH];
    if (!output_file || output_file[0] == '\0') {
        if (target_type == KRT_TARGET_CMD_KRO) {
            strcpy(default_output, "output.exe");
            output_file = default_output;
        } else {
            KrtError("输出文件为空");
            return 1;
        }
    }
    KrtConfig* config = KrtConfigCreate();
    if (!config) {
        KrtError("无法创建配置管理器");
        return 1;
    }
    config->keep_temp_files = keep_temp;
    switch (target_type) {
        case KRT_TARGET_CMD_ASM:
            config->target_type = KRT_TARGET_ASM;
            break;
        case KRT_TARGET_CMD_IR:
            config->target_type = KRT_TARGET_IR;
            break;
        case KRT_TARGET_CMD_EXE:
            config->target_type = KRT_TARGET_EXE;
            break;
        case KRT_TARGET_CMD_VM:
            config->target_type = KRT_TARGET_VM;
            break;
        case KRT_TARGET_CMD_KRO:
            config->target_type = KRT_TARGET_KRO;
            break;
        case KRT_TARGET_CMD_BUILD:
            
            switch (target_type) {
                case KRT_TARGET_CMD_ASM:
                    config->target_type = KRT_TARGET_ASM;
                    break;
                case KRT_TARGET_CMD_IR:
                    config->target_type = KRT_TARGET_IR;
                    break;
                case KRT_TARGET_CMD_EXE:
                    config->target_type = KRT_TARGET_EXE;
                    break;
                case KRT_TARGET_CMD_VM:
                    config->target_type = KRT_TARGET_VM;
                    break;
                case KRT_TARGET_CMD_KRO:
                    config->target_type = KRT_TARGET_KRO;
                    break;
                default:
                    config->target_type = KRT_TARGET_EXE;
                    break;
            }
            break;
        case KRT_TARGET_CMD_CLEAN:
            break;
        case KRT_TARGET_CMD_CHECK:
            break;
        case KRT_TARGET_CMD_VERSION:
            break;
    }
    
    config->show_ir = show_ir;
    
    KrtPlatform* platform = KrtPlatformGetCurrent();
    if (!platform) {
        KrtError("无法创建平台抽象");
        KrtConfigDestroy(config);
        return 1;
    }
    
    KrtCompilePipeline* pipeline = KrtCompilePipelineCreate(config, platform);
    if (!pipeline) {
        KrtError("无法创建编译管道");
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    const char* compile_output = output_file;
#ifndef __linux__
    char temp_eo[KRT_MAX_PATH];
#endif
    char temp_kro[KRT_MAX_PATH];
    if (target_type == KRT_TARGET_CMD_EXE || target_type == KRT_TARGET_CMD_BUILD) {
        char file_base[KRT_MAX_PATH];
        KrtPathGetFilename(input_file, file_base, sizeof(file_base));
        KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
        char file_base_truncated[1010];
        strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
        file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
#ifndef __linux__
        snprintf(temp_eo, sizeof(temp_eo), "%s.eo", file_base_truncated);
        compile_output = temp_eo;
#else
        snprintf(temp_kro, sizeof(temp_kro), "%s.kro", file_base_truncated);
        compile_output = temp_kro;
#endif
    } else if (target_type == KRT_TARGET_CMD_KRO) {
        
        char file_base[KRT_MAX_PATH];
        KrtPathGetFilename(input_file, file_base, sizeof(file_base));
        KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
        char file_base_truncated[1010];
        strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
        file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
        snprintf(temp_kro, sizeof(temp_kro), "%s.kro", file_base_truncated);
        compile_output = temp_kro;
    }

    int result = KrtCompilePipelineExecute(pipeline, input_file, compile_output);
    double total_duration = pipeline->total_duration;

    if (result) {
        if (target_type == KRT_TARGET_CMD_EXE || target_type == KRT_TARGET_CMD_BUILD) {
            if (pipeline->compiler) {
                KrtCompilerDestroy(pipeline->compiler);
                pipeline->compiler = NULL;
            }

#ifdef __linux__
            if (KrtLinkKroExecutable(config, platform, compile_output, output_file)) {
                KrtTaskReport("build", input_file, KRT_TASK_RESULT_EXECUTED, total_duration, KrtGetGlobalTaskStats());
            } else {
                KrtTaskReport("build", input_file, KRT_TASK_RESULT_FAILED, total_duration, KrtGetGlobalTaskStats());
                result = 0;
            }
#else
            KrtBuildContext* build_ctx = KrtBuildContextCreate(config, platform);
            if (build_ctx) {
                if (KrtBuildExecute(build_ctx, temp_eo, output_file)) {
                    KrtTaskReport("build", input_file, KRT_TASK_RESULT_EXECUTED, total_duration, KrtGetGlobalTaskStats());
                } else {
                    KrtError("构建失败：%s", KrtBuildGetError(build_ctx));
                    KrtTaskReport("build", input_file, KRT_TASK_RESULT_FAILED, total_duration, KrtGetGlobalTaskStats());
                    result = 0;
                }
                KrtBuildContextDestroy(build_ctx);
            } else {
                KrtError("无法创建构建上下文");
                result = 0;
            }
#endif
        } else if (target_type == KRT_TARGET_CMD_KRO) {
            if (pipeline->compiler) {
                KrtCompilerDestroy(pipeline->compiler);
                pipeline->compiler = NULL;
            }
            KrtTaskReport("compile", input_file, KRT_TASK_RESULT_EXECUTED, total_duration, KrtGetGlobalTaskStats());

            const char* kro_file = compile_output;

            char default_exe_output[KRT_MAX_PATH];
            if (!output_file || output_file[0] == '\0') {
                strncpy(default_exe_output, kro_file, sizeof(default_exe_output) - 1);
                default_exe_output[sizeof(default_exe_output) - 1] = '\0';

                char* dot = strrchr(default_exe_output, '.');
                if (dot) {
                    strcpy(dot, ".exe");
                } else {
                    strncat(default_exe_output, ".exe", sizeof(default_exe_output) - strlen(default_exe_output) - 1);
                }
                output_file = default_exe_output;
            }
            const char* exe_output = output_file;

            if (!KrtLinkKroExecutable(config, platform, kro_file, exe_output)) {
                result = 0;
            }
        } else if (target_type == KRT_TARGET_CMD_VM) {
            KrtTaskReport("compile", input_file, KRT_TASK_RESULT_EXECUTED, total_duration, KrtGetGlobalTaskStats());
        } else {
            KrtTaskReport("compile", input_file, KRT_TASK_RESULT_EXECUTED, total_duration, KrtGetGlobalTaskStats());
        }
    } else {
        KrtError("编译失败: %s", pipeline->error_message);
        KrtTaskReport("compile", input_file, KRT_TASK_RESULT_FAILED, total_duration, KrtGetGlobalTaskStats());
    }
    
    KrtCompilePipelineDestroy(pipeline);
    KrtPlatformDestroy(platform);
    KrtConfigDestroy(config);
    
    return result ? 0 : 1;
}

static void KrtGetExecutableDir(char* buffer, int buffer_size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buffer, buffer_size);
    char* last_sep = strrchr(buffer, '\\');
    if (last_sep) {
        *last_sep = '\0';
    }
#else
    ssize_t len = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if (len != -1) {
        buffer[len] = '\0';
        char* last_sep = strrchr(buffer, '/');
        if (last_sep) {
            *last_sep = '\0';
        }
    } else {
        strcpy(buffer, ".");
    }
#endif
}

static int KrtScanStdlibDir(const char* dir_path, char** file_list, int max_files, int* file_count) {
#ifdef _WIN32
    char search_path[KRT_MAX_PATH];
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
        
        char full_path[KRT_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            
            KrtScanStdlibDir(full_path, file_list, max_files, file_count);
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
        
        char full_path[KRT_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                
                KrtScanStdlibDir(full_path, file_list, max_files, file_count);
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

static void KrtGetKroFilename(const char* krt_file, char* kro_file, int kro_file_size) {
    strncpy(kro_file, krt_file, kro_file_size - 1);
    kro_file[kro_file_size - 1] = '\0';
    
    int len = strlen(kro_file);
    if (len > 4) {
        strcpy(kro_file + len - 4, ".kro");
    }
}

static int KrtNeedsRecompile(const char* krt_file, const char* kro_file) {
    struct stat krt_stat, kro_stat;
    
    if (stat(krt_file, &krt_stat) != 0) {
        return 0;  
    }
    
    if (stat(kro_file, &kro_stat) != 0) {
        return 1;  
    }
    
#ifdef _WIN32
    return kro_stat.st_mtime < krt_stat.st_mtime;
#else
    return kro_stat.st_mtime < krt_stat.st_mtime;
#endif
}

static int KrtCompileStdlib(KrtConfig* config, KrtPlatform* platform, 
                             char** stdlib_krt_files, int stdlib_count,
                             char** stdlib_kro_files, int max_kro_files) {
    int compiled_count = 0;
    
    for (int i = 0; i < stdlib_count && compiled_count < max_kro_files; i++) {
        char kro_file[KRT_MAX_PATH];
        KrtGetKroFilename(stdlib_krt_files[i], kro_file, sizeof(kro_file));
        
        if (!KrtNeedsRecompile(stdlib_krt_files[i], kro_file)) {
            stdlib_kro_files[compiled_count] = strdup(kro_file);
            compiled_count++;
            continue;
        }

        KrtCompilePipeline* pipeline = KrtCompilePipelineCreate(config, platform);
        if (!pipeline) {
            KrtError("Failed to create compile pipeline for %s", stdlib_krt_files[i]);
            continue;
        }

        int compile_result = KrtCompilePipelineExecute(pipeline, stdlib_krt_files[i], kro_file);

        if (compile_result && pipeline->compiler) {
            stdlib_kro_files[compiled_count] = strdup(kro_file);
            compiled_count++;
        } else {
            KrtError("Failed to compile stdlib %s: %s", stdlib_krt_files[i], pipeline->error_message);
        }

        KrtCompilePipelineDestroy(pipeline);
    }
    
    return compiled_count;
}

typedef struct {
    const char* file_path;
    ASTNode* ast;
    Lexer* lexer;
    Parser* parser;
    char* source;
} ParsedFile;

static int KrtCompileMultipleFiles(const char** input_files, int input_count, const char* output_file, 
                                    KrtCommandTargetType target_type, int show_ir, int keep_temp) {
    if (!input_files || input_count <= 0) {
        KrtError("No input files specified");
        return 1;
    }
    
    if (input_count == 1) {
        
        return KrtCompileSingleFile(input_files[0], output_file, target_type, show_ir, keep_temp);
    }
    
    if (target_type != KRT_TARGET_CMD_KRO) {
        KrtError("Multiple file compilation only supported with 'target eo'");
        return 1;
    }

    KrtConfig* config = KrtConfigCreate();
    if (!config) {
        KrtError("Failed to create config");
        return 1;
    }
    config->keep_temp_files = keep_temp;
    config->target_type = KRT_TARGET_KRO;
    config->show_ir = show_ir;
    
    KrtPlatform* platform = KrtPlatformGetCurrent();
    if (!platform) {
        KrtError("Failed to create platform abstraction");
        KrtConfigDestroy(config);
        return 1;
    }
    
    char default_output[KRT_MAX_PATH];
    if (!output_file || output_file[0] == '\0') {
        char file_base[KRT_MAX_PATH];
        KrtPathGetFilename(input_files[0], file_base, sizeof(file_base));
        KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
        char file_base_truncated[1010];
        strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
        file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
        snprintf(default_output, sizeof(default_output), "%s.exe", file_base_truncated);
        output_file = default_output;
    }
    
    ParsedFile* parsed_files = (ParsedFile*)calloc(input_count, sizeof(ParsedFile));
    if (!parsed_files) {
        KrtError("Failed to allocate memory");
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    int parsed_count = 0;
    for (int i = 0; i < input_count; i++) {
        FILE* fp = fopen(input_files[i], "r");
        if (!fp) {
            KrtError("Cannot open file: %s", input_files[i]);
            continue;
        }
        
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        rewind(fp);
        
        char* source = (char*)malloc(file_size + 1);
        if (!source) {
            fclose(fp);
            continue;
        }
        
        fread(source, 1, file_size, fp);
        source[file_size] = '\0';
        fclose(fp);
        
        Lexer* lexer = lexer_create(source);
        if (!lexer) {
            free(source);
            continue;
        }
        
        Parser* parser = parser_create(lexer);
        if (!parser) {
            free(source);
            lexer_destroy(lexer);
            continue;
        }
        
        ASTNode* ast = parser_parse(parser);
        if (!ast) {
            free(source);
            lexer_destroy(lexer);
            parser_destroy(parser);
            continue;
        }
        
        parsed_files[parsed_count].file_path = input_files[i];
        parsed_files[parsed_count].ast = ast;
        parsed_files[parsed_count].lexer = lexer;
        parsed_files[parsed_count].parser = parser;
        parsed_files[parsed_count].source = source;
        parsed_count++;
    }
    
    if (parsed_count == 0) {
        KrtError("All files failed to parse");
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    ASTNode* merged_ast = NULL;
    for (int i = 0; i < parsed_count; i++) {
        ASTNode* ast = parsed_files[i].ast;
        if (!merged_ast) {
            
            merged_ast = ast_create_node(AST_PROGRAM, 0, 0);
            if (!merged_ast) continue;
            merged_ast->data.block.statements = NULL;
            merged_ast->data.block.statement_count = 0;
        }
        
        if (ast->type == AST_PROGRAM) {
            int old_count = merged_ast->data.block.statement_count;
            int new_count = ast->data.block.statement_count;
            ASTNode** new_statements = (ASTNode**)realloc(merged_ast->data.block.statements, 
                                                           (old_count + new_count) * sizeof(ASTNode*));
            if (new_statements) {
                merged_ast->data.block.statements = new_statements;
                for (int j = 0; j < new_count; j++) {
                    merged_ast->data.block.statements[old_count + j] = ast->data.block.statements[j];
                }
                merged_ast->data.block.statement_count = old_count + new_count;
            }
        }
    }
    
    if (!merged_ast) {
        KrtError("Failed to merge ASTs");
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    {
        char stdlib_dir_check[KRT_MAX_PATH];
        char project_dir_check[KRT_MAX_PATH];
        
        strncpy(project_dir_check, input_files[0], sizeof(project_dir_check) - 1);
        project_dir_check[sizeof(project_dir_check) - 1] = '\0';
        char* sep = strrchr(project_dir_check, '\\');
        if (!sep) sep = strrchr(project_dir_check, '/');
        if (sep) {
            *sep = '\0';
        } else {
            strcpy(project_dir_check, ".");
        }
        
        char project_dir_truncated[1010];
        strncpy(project_dir_truncated, project_dir_check, sizeof(project_dir_truncated) - 1);
        project_dir_truncated[sizeof(project_dir_truncated) - 1] = '\0';
        snprintf(stdlib_dir_check, sizeof(stdlib_dir_check), "%s\\stdlib", project_dir_truncated);
        
        struct stat stdlib_stat_check;
        int stdlib_found_check = 0;
        
        if (stat(stdlib_dir_check, &stdlib_stat_check) == 0 && (stdlib_stat_check.st_mode & S_IFDIR)) {
            stdlib_found_check = 1;
        } else {
            char exe_dir_check[KRT_MAX_PATH];
            KrtGetExecutableDir(exe_dir_check, sizeof(exe_dir_check));
            char exe_dir_truncated[1010];
            strncpy(exe_dir_truncated, exe_dir_check, sizeof(exe_dir_truncated) - 1);
            exe_dir_truncated[sizeof(exe_dir_truncated) - 1] = '\0';
            snprintf(stdlib_dir_check, sizeof(stdlib_dir_check), "%s\\..\\stdlib", exe_dir_truncated);

            if (stat(stdlib_dir_check, &stdlib_stat_check) == 0 && (stdlib_stat_check.st_mode & S_IFDIR)) {
                stdlib_found_check = 1;
            }
        }
        
        if (stdlib_found_check) {
            char** stdlib_krt_files_check = (char**)malloc(256 * sizeof(char*));
            int stdlib_count_check = 0;

            if (stdlib_krt_files_check) {
                KrtScanStdlibDir(stdlib_dir_check, stdlib_krt_files_check, 256, &stdlib_count_check);

                for (int i = 0; i < stdlib_count_check; i++) {
                    FILE* fp = fopen(stdlib_krt_files_check[i], "r");
                    if (!fp) {
                        continue;
                    }
                    
                    fseek(fp, 0, SEEK_END);
                    long file_size = ftell(fp);
                    rewind(fp);
                    
                    char* source = (char*)malloc(file_size + 1);
                    if (!source) {
                        fclose(fp);
                        continue;
                    }
                    
                    fread(source, 1, file_size, fp);
                    source[file_size] = '\0';
                    fclose(fp);
                    
                    Lexer* lexer = lexer_create(source);
                    if (!lexer) {
                        free(source);
                        continue;
                    }
                    
                    Parser* parser = parser_create(lexer);
                    if (!parser) {
                        free(source);
                        lexer_destroy(lexer);
                        continue;
                    }
                    
                    ASTNode* ast = parser_parse(parser);
                    if (!ast) {
                        free(source);
                        lexer_destroy(lexer);
                        parser_destroy(parser);
                        continue;
                    }
                    
                    if (ast->type == AST_PROGRAM) {
                        int old_count = merged_ast->data.block.statement_count;
                        int new_count = ast->data.block.statement_count;
                        ASTNode** new_statements = (ASTNode**)realloc(merged_ast->data.block.statements, 
                                                                       (old_count + new_count) * sizeof(ASTNode*));
                        if (new_statements) {
                            merged_ast->data.block.statements = new_statements;
                            for (int j = 0; j < new_count; j++) {
                                merged_ast->data.block.statements[old_count + j] = ast->data.block.statements[j];
                            }
                            merged_ast->data.block.statement_count = old_count + new_count;
                        }
                    }
                }

                for (int i = 0; i < stdlib_count_check; i++) {
                    if (stdlib_krt_files_check[i]) free(stdlib_krt_files_check[i]);
                }
                free(stdlib_krt_files_check);
            }
        }
    }
    
    SemanticAnalyzer* semantic_analyzer = semantic_analyzer_create();
    if (!semantic_analyzer) {
        KrtError("Failed to create semantic analyzer");
        ast_destroy_node(merged_ast);
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    SemanticAnalysisResult* semantic_result = semantic_analyzer_analyze(semantic_analyzer, merged_ast);
    if (!semantic_result || !semantic_result->success) {
        KrtError("Semantic analysis failed");
        semantic_analyzer_destroy(semantic_analyzer);
        ast_destroy_node(merged_ast);
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    TypeCheckContext* type_context = type_check_context_create(semantic_analyzer);
    if (!type_context) {
        KrtError("Failed to create type check context");
        KRT_FREE(semantic_result);
        semantic_analyzer_destroy(semantic_analyzer);
        ast_destroy_node(merged_ast);
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    if (type_check_program(type_context, merged_ast) == 0) {
        KrtError("Type checking failed");
        type_check_context_destroy(type_context);
        KRT_FREE(semantic_result);
        semantic_analyzer_destroy(semantic_analyzer);
        ast_destroy_node(merged_ast);
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }

    char** kro_files = (char**)malloc(parsed_count * sizeof(char*));
    if (!kro_files) {
        KrtError("Failed to allocate memory");
        type_check_context_destroy(type_context);
        KRT_FREE(semantic_result);
        semantic_analyzer_destroy(semantic_analyzer);
        ast_destroy_node(merged_ast);
        for (int i = 0; i < parsed_count; i++) {
            free(parsed_files[i].source);
            lexer_destroy(parsed_files[i].lexer);
            parser_destroy(parsed_files[i].parser);
        }
        free(parsed_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    int success_count = 0;
    for (int i = 0; i < parsed_count; i++) {
        const char* input_file = parsed_files[i].file_path;
        
        char file_base[KRT_MAX_PATH];
        KrtPathGetFilename(input_file, file_base, sizeof(file_base));
        KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
        
        kro_files[i] = (char*)malloc(KRT_MAX_PATH);
        if (!kro_files[i]) {
            KrtError("Failed to allocate memory");
            continue;
        }
        char file_base_truncated[1010];
        strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
        file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
        snprintf(kro_files[i], KRT_MAX_PATH, "%s.kro", file_base_truncated);

        ASTNode* file_ast = ast_create_node(AST_PROGRAM, 0, 0);
        if (!file_ast) {
            KrtError("Failed to create AST for %s", input_file);
            free(kro_files[i]);
            kro_files[i] = NULL;
            continue;
        }
        
        ASTNode* original_ast = parsed_files[i].ast;
        if (original_ast->type == AST_PROGRAM) {
            int stmt_count = original_ast->data.block.statement_count;
            file_ast->data.block.statements = (ASTNode**)malloc(stmt_count * sizeof(ASTNode*));
            if (file_ast->data.block.statements) {
                for (int j = 0; j < stmt_count; j++) {
                    file_ast->data.block.statements[j] = original_ast->data.block.statements[j];
                }
                file_ast->data.block.statement_count = stmt_count;
            }
        }
        
        KrtCompiler* compiler = KrtCompilerCreate(kro_files[i], KRT_TARGET_KRO_OBJ);
        if (!compiler) {
            KrtError("Failed to create compiler for %s", input_file);
            ast_destroy_node(file_ast);
            free(kro_files[i]);
            kro_files[i] = NULL;
            continue;
        }
        
        KrtCompilerCompile(compiler, file_ast, type_context);
        
        KrtCompilerDestroy(compiler);
        ast_destroy_node(file_ast);

        success_count++;
    }
    
    type_check_context_destroy(type_context);
    KRT_FREE(semantic_result);
    semantic_analyzer_destroy(semantic_analyzer);
    
    for (int i = 0; i < parsed_count; i++) {
        free(parsed_files[i].source);
        lexer_destroy(parsed_files[i].lexer);
        parser_destroy(parsed_files[i].parser);
    }
    free(parsed_files);
    
    if (success_count == 0) {
        KrtError("All files failed to compile");
        for (int i = 0; i < parsed_count; i++) {
            if (kro_files[i]) free(kro_files[i]);
        }
        free(kro_files);
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    char stdlib_dir[KRT_MAX_PATH];
    char project_dir[KRT_MAX_PATH];
    
    strncpy(project_dir, input_files[0], sizeof(project_dir) - 1);
    project_dir[sizeof(project_dir) - 1] = '\0';
    char* last_sep = strrchr(project_dir, '\\');
    if (!last_sep) last_sep = strrchr(project_dir, '/');
    if (last_sep) {
        *last_sep = '\0';
    } else {
        strcpy(project_dir, ".");
    }
    
    char project_dir_truncated[1010];
    strncpy(project_dir_truncated, project_dir, sizeof(project_dir_truncated) - 1);
    project_dir_truncated[sizeof(project_dir_truncated) - 1] = '\0';
    snprintf(stdlib_dir, sizeof(stdlib_dir), "%s\\stdlib", project_dir_truncated);
    
    struct stat stdlib_stat;
    int stdlib_found = 0;
    
    if (stat(stdlib_dir, &stdlib_stat) == 0 && (stdlib_stat.st_mode & S_IFDIR)) {
        stdlib_found = 1;
    } else {
        char exe_dir[KRT_MAX_PATH];
        KrtGetExecutableDir(exe_dir, sizeof(exe_dir));
        char exe_dir_truncated[1010];
        strncpy(exe_dir_truncated, exe_dir, sizeof(exe_dir_truncated) - 1);
        exe_dir_truncated[sizeof(exe_dir_truncated) - 1] = '\0';
        snprintf(stdlib_dir, sizeof(stdlib_dir), "%s" KRT_PATH_SEPARATOR_STR ".." KRT_PATH_SEPARATOR_STR "stdlib", exe_dir_truncated);

        if (stat(stdlib_dir, &stdlib_stat) == 0 && (stdlib_stat.st_mode & S_IFDIR)) {
            stdlib_found = 1;
        }
    }

    char** stdlib_krt_files = NULL;
    char** stdlib_kro_files = NULL;
    int stdlib_count = 0;
    int stdlib_kro_count = 0;

    if (stdlib_found) {
        stdlib_krt_files = (char**)malloc(256 * sizeof(char*));
        stdlib_kro_files = (char**)malloc(256 * sizeof(char*));

        if (stdlib_krt_files && stdlib_kro_files) {
            KrtScanStdlibDir(stdlib_dir, stdlib_krt_files, 256, &stdlib_count);

            if (stdlib_count > 0) {
                stdlib_kro_count = KrtCompileStdlib(config, platform,
                                                     stdlib_krt_files, stdlib_count,
                                                     stdlib_kro_files, 256);
            }
        }
    }
    
    int total_obj_count = success_count + stdlib_kro_count;
    const char** obj_files = (const char**)malloc(total_obj_count * sizeof(const char*));
    if (!obj_files) {
        KrtError("Failed to allocate memory");
        for (int i = 0; i < parsed_count; i++) {
            if (kro_files[i]) free(kro_files[i]);
        }
        free(kro_files);
        if (stdlib_krt_files) {
            for (int i = 0; i < stdlib_count; i++) {
                if (stdlib_krt_files[i]) free(stdlib_krt_files[i]);
            }
            free(stdlib_krt_files);
        }
        if (stdlib_kro_files) {
            for (int i = 0; i < stdlib_kro_count; i++) {
                if (stdlib_kro_files[i]) free(stdlib_kro_files[i]);
            }
            free(stdlib_kro_files);
        }
        KrtConfigDestroy(config);
        KrtPlatformDestroy(platform);
        return 1;
    }
    
    int obj_count = 0;
    
    for (int i = 0; i < parsed_count; i++) {
        if (kro_files[i] && KrtPlatformPathExists(platform, kro_files[i])) {
            obj_files[obj_count++] = kro_files[i];
        }
    }
    
    for (int i = 0; i < stdlib_kro_count; i++) {
        if (stdlib_kro_files[i] && KrtPlatformPathExists(platform, stdlib_kro_files[i])) {
            obj_files[obj_count++] = stdlib_kro_files[i];
        }
    }
    
    int link_result = KrtArkLinkLinkObjects(obj_files, obj_count, output_file, config);

    if (link_result == 0) {
        KrtTaskReport("link", output_file, KRT_TASK_RESULT_EXECUTED, 0.0, KrtGetGlobalTaskStats());
    } else {
        KrtError("Linking failed");
        KrtTaskReport("link", output_file, KRT_TASK_RESULT_FAILED, 0.0, KrtGetGlobalTaskStats());
    }
    
    for (int i = 0; i < parsed_count; i++) {
        if (kro_files[i]) {
            if (!keep_temp) {
                KrtDeleteFile(kro_files[i]);
            }
            free(kro_files[i]);
        }
    }
    free(kro_files);
    
    if (stdlib_krt_files) {
        for (int i = 0; i < stdlib_count; i++) {
            if (stdlib_krt_files[i]) free(stdlib_krt_files[i]);
        }
        free(stdlib_krt_files);
    }
    if (stdlib_kro_files) {
        for (int i = 0; i < stdlib_kro_count; i++) {
            if (stdlib_kro_files[i]) {
                free(stdlib_kro_files[i]);
            }
        }
        free(stdlib_kro_files);
    }
    
    free(obj_files);
    KrtConfigDestroy(config);
    KrtPlatformDestroy(platform);
    
    return link_result == 0 ? 0 : 1;
}

static int KrtBuildProject(const char* project_file, const char* output_path __attribute__((unused)), int keep_temp) {
    
    KrtPlatform* platform = KrtPlatformGetCurrent();
    if (!platform) {
        KrtError("无法创建平台抽象");
        return 1;
    }
    
    KrtProject* project = KrtProjLoad(project_file);
    if (!project) {
        KrtError("无法加载项目文件: %s", project_file);
        return 1;
    }
    
    int max_threads = 8;
    KrtConfig* config = KrtConfigCreate();
    if (config) config->keep_temp_files = keep_temp;
    
    ParallelCompiler* parallel_compiler = ParallelCompilerCreate(max_threads, config);
    if (!parallel_compiler) {
        KrtError("无法创建并行编译器");
        KrtProjDestroy(project);
        if (config) KrtConfigDestroy(config);
        return 1;
    }
    
    char project_root[KRT_MAX_PATH];
    char* last_sep = strrchr(project_file, '\\');
    if (!last_sep) last_sep = strrchr(project_file, '/');
    if (last_sep) {
        size_t dir_len = last_sep - project_file;
        strncpy(project_root, project_file, dir_len);
        project_root[dir_len] = '\0';
    } else {
        strcpy(project_root, ".");
    }
    
    char obj_dir[KRT_MAX_PATH];
    KrtPlatformPathJoin(platform, obj_dir, sizeof(obj_dir), project_root, "obj");
    KrtEnsureDirectoryRecursive(obj_dir);

    KrtProjectItem* item = project->items;
    while (item) {
        if (strcmp(item->item_type, "Compile") == 0) {
            char source_path[KRT_MAX_PATH];
            KrtPlatformPathJoin(platform, source_path, sizeof(source_path), project_root, item->file_path);
            
            if (!KrtPlatformPathExists(platform, source_path)) {
                KrtError("源文件不存在: %s", source_path);
                ParallelCompilerDestroy(parallel_compiler);
                KrtProjDestroy(project);
                KrtConfigDestroy(config);
                return 1;
            }
            
            char file_name[KRT_MAX_PATH];
            char* name_start = strrchr(source_path, '\\');
            if (!name_start) name_start = strrchr(source_path, '/');
            if (name_start) {
                strcpy(file_name, name_start + 1);
            } else {
                strcpy(file_name, source_path);
            }
            char file_name_no_ext[KRT_MAX_PATH];
            KrtPathRemoveExtension(file_name, file_name_no_ext, sizeof(file_name_no_ext));
            
            char intermediate_asm[KRT_MAX_PATH];
            char intermediate_obj[KRT_MAX_PATH];
            
            char obj_dir_truncated[500];
            strncpy(obj_dir_truncated, obj_dir, sizeof(obj_dir_truncated) - 1);
            obj_dir_truncated[sizeof(obj_dir_truncated) - 1] = '\0';
            char file_name_truncated[500];
            strncpy(file_name_truncated, file_name_no_ext, sizeof(file_name_truncated) - 1);
            file_name_truncated[sizeof(file_name_truncated) - 1] = '\0';
            snprintf(intermediate_asm, sizeof(intermediate_asm), "%s%c%s.asm",
                    obj_dir_truncated, KrtPlatformGetSeparator(platform), file_name_truncated);
            snprintf(intermediate_obj, sizeof(intermediate_obj), "%s%c%s.obj",
                    obj_dir_truncated, KrtPlatformGetSeparator(platform), file_name_truncated);
            
            if (ParallelCompilerAddFile(parallel_compiler, source_path, intermediate_asm, intermediate_obj, 
                                         KRT_TARGET_ASM, 0) != 0) {
                KrtError("添加编译任务失败: %s", source_path);
                ParallelCompilerDestroy(parallel_compiler);
                KrtProjDestroy(project);
                KrtConfigDestroy(config);
                return 1;
            }
        }
        item = item->next;
    }
    
    ParallelCompilerCollectGenericTypes(parallel_compiler);

    int compile_result = ParallelCompilerExecute(parallel_compiler);
    
    int total_files = 0, succeeded_files = 0, failed_files = 0;
    ParallelCompilerGetStats(parallel_compiler, &total_files, &succeeded_files, &failed_files);
    
    if (compile_result != 0 || failed_files > 0) {
        KrtError("并行编译失败：%d 个文件编译失败", failed_files);
        ParallelCompilerDestroy(parallel_compiler);
        KrtProjDestroy(project);
        return 1;
    }
    
    if (total_files > 0) {
        char bin_dir[KRT_MAX_PATH];
        char platform_name[32];
        
        strncpy(platform_name, platform->name, sizeof(platform_name));
        for (int i = 0; platform_name[i]; i++) {
            platform_name[i] = tolower(platform_name[i]);
        }
        
        char bin_subdir[KRT_MAX_PATH];
        snprintf(bin_subdir, sizeof(bin_subdir), "bin%c%s", 
                KrtPlatformGetSeparator(platform), platform_name);
        
        KrtPlatformPathJoin(platform, bin_dir, sizeof(bin_dir), project_root, bin_subdir);
        
        KrtEnsureDirectoryRecursive(bin_dir);
        
        char project_output_path[KRT_MAX_PATH];
        char project_name[KRT_MAX_PATH];
        KrtPathGetFilename(project_file, project_name, sizeof(project_name));
        KrtPathRemoveExtension(project_name, project_name, sizeof(project_name));
        
        char project_filename[KRT_MAX_PATH];
        if (platform->type == KRT_CONFIG_PLATFORM_WINDOWS) {
            char project_name_truncated[1010];
            strncpy(project_name_truncated, project_name, sizeof(project_name_truncated) - 1);
            project_name_truncated[sizeof(project_name_truncated) - 1] = '\0';
            snprintf(project_filename, sizeof(project_filename), "%s.exe", project_name_truncated);
        } else {
            strncpy(project_filename, project_name, sizeof(project_filename));
        }
        
        KrtPlatformPathJoin(platform, project_output_path, sizeof(project_output_path), bin_dir, project_filename);

        if (ParallelCompilerLinkResults(parallel_compiler, project_output_path) != 0) {
            KrtError("链接失败");
            ParallelCompilerDestroy(parallel_compiler);
            KrtProjDestroy(project);
            KrtConfigDestroy(config);
            return 1;
        }
    }
    
    KrtPlatformDestroy(platform);
    ParallelCompilerDestroy(parallel_compiler);
    KrtProjDestroy(project);
    
    return 0;
}

static int KrtRunCompiler(const char** input_files, int input_count, const char* output_file, KrtCommandTargetType target_type, int show_ir, int keep_temp) {
    if (!input_files || input_count <= 0) {
        KrtError("未指定输入文件");
        return 1;
    }
    
    int is_project = 0;
    const char* ext = strrchr(input_files[0], '.');
    if (ext && (strcmp(ext, ".esproj") == 0 || strcmp(ext, ".json") == 0)) {
        is_project = 1;
    }
    
    char default_output[KRT_MAX_PATH];
    const char* final_output = output_file ? output_file : default_output;
    
    memset(default_output, 0, sizeof(default_output));
    if (is_project) {
        char project_name[KRT_MAX_PATH];
        KrtPathGetFilename(input_files[0], project_name, sizeof(project_name));
        KrtPathRemoveExtension(project_name, project_name, sizeof(project_name));
#ifdef _WIN32
        char project_name_truncated[1010];
        strncpy(project_name_truncated, project_name, sizeof(project_name_truncated) - 1);
        project_name_truncated[sizeof(project_name_truncated) - 1] = '\0';
        snprintf(default_output, sizeof(default_output), "%s.exe", project_name_truncated);
#else
        snprintf(default_output, sizeof(default_output), "%s", project_name);
#endif
    } else {
        switch (target_type) {
            case KRT_TARGET_CMD_EXE: {
                char file_base[KRT_MAX_PATH];
                KrtPathGetFilename(input_files[0], file_base, sizeof(file_base));
                KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
#ifdef _WIN32
                char file_base_truncated[1010];
                strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
                file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
                snprintf(default_output, sizeof(default_output), "%s.exe", file_base_truncated);
#else
                snprintf(default_output, sizeof(default_output), "%s", file_base);
#endif
                break;
            }
            case KRT_TARGET_CMD_IR:
                strncpy(default_output, "output.ir", sizeof(default_output) - 1);
                break;
            case KRT_TARGET_CMD_BUILD: {
                char file_base[KRT_MAX_PATH];
                KrtPathGetFilename(input_files[0], file_base, sizeof(file_base));
                KrtPathRemoveExtension(file_base, file_base, sizeof(file_base));
#ifdef _WIN32
                char file_base_truncated[1010];
                strncpy(file_base_truncated, file_base, sizeof(file_base_truncated) - 1);
                file_base_truncated[sizeof(file_base_truncated) - 1] = '\0';
                snprintf(default_output, sizeof(default_output), "%s.exe", file_base_truncated);
#else
                snprintf(default_output, sizeof(default_output), "%s", file_base);
#endif
                break;
            }
            case KRT_TARGET_CMD_VM:
                strncpy(default_output, "output.ebc", sizeof(default_output) - 1);
                break;
            case KRT_TARGET_CMD_KRO:
                strncpy(default_output, "output.exe", sizeof(default_output) - 1);
                break;
            default:
                strncpy(default_output, "output.asm", sizeof(default_output) - 1);
                break;
        }
    }
    
    if (is_project) {
        return KrtBuildProject(input_files[0], final_output, keep_temp);
    } else if (input_count > 1) {
        
        return KrtCompileMultipleFiles(input_files, input_count, final_output, target_type, show_ir, keep_temp);
    } else {
        
        return KrtCompileSingleFile(input_files[0], final_output, target_type, show_ir, keep_temp);
    }
}

int main(int argc, char* argv[]) {
    fflush(stdout);
    KrtConsoleSetColorEnabled(KrtConsoleSupportsColor());
    KrtBuildSummaryReset();
    
    double total_start = KrtGetTime();
    
    KrtCommandLineOptions options = KrtParseCommandLine(argc, argv);
    
    if (options.show_help) {
        KrtPrintUsage(argv[0]);
        KrtOutputCacheCleanup();
        return 0;
    }
    
    if (options.create_project) {
        const char* project_name = project_name_from_type(options.project_type);
        KrtCreateProject(project_name, options.project_type);
        KrtBuildSummarySetDuration(KrtGetTime() - total_start);
        KrtPrintBuildSummary();
        KrtOutputCacheCleanup();
        return 0;
    }
    
    if (options.target_type == KRT_TARGET_CMD_VERSION) {
        const char *blue  = KrtColor(KRT_COL_BLUE);
        const char *gray  = KrtColor(KRT_COL_GRAY);
        const char *reset = KrtColor(KRT_COL_RESET);

        KrtPrintf("%sKairote Lang Compiler Version%s: %s %s (%s %s)\n",
                  blue, reset,
                  KRT_COMPILER_VERSION,
                  gray, __DATE__, __TIME__);
        KrtOutputCacheCleanup();
        return 0;
    }

    int result = KrtRunCompiler(options.input_files, options.input_file_count, options.output_file, options.target_type, options.show_ir, options.keep_temp_files);

    KrtBuildSummarySetDuration(KrtGetTime() - total_start);
    if (result != 0) {
        KrtBuildSummarySetFailed(1);
    }
    KrtPrintBuildSummary();
    
    KrtOutputCacheCleanup();
    return result;
}