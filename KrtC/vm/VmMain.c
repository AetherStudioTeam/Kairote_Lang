#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "VmExecutor.h"
#include "Core/Utils/Logger.h"

static void print_usage(const char* program_name) {
    printf("用法: %s [选项] <字节码文件>\n", program_name);
    printf("选项:\n");
    printf("  -v, --verbose    显示详细输出\n");
    printf("  -h, --help       显示此帮助信息\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* bytecode_file = NULL;
    int verbose = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            if (bytecode_file == NULL) {
                bytecode_file = argv[i];
            } else {
                printf("错误: 多个输入文件\n");
                return 1;
            }
        } else {
            printf("错误: 未知选项 %s\n", argv[i]);
            return 1;
        }
    }
    
    if (bytecode_file == NULL) {
        printf("错误: 未指定字节码文件\n");
        return 1;
    }
    
    VMExecutor* executor = vm_executor_create(bytecode_file, verbose);
    if (!executor) {
        printf("错误: 无法创建VM执行器\n");
        return 1;
    }
    
    KrtInterpretResult result = vm_executor_execute(executor);
    
    vm_executor_destroy(executor);
    
    switch (result) {
        case INTERPRET_OK:
            if (verbose) {
                printf("程序执行成功\n");
            }
            return 0;
        case INTERPRET_COMPILE_ERROR:
            printf("错误: 字节码加载失败\n");
            return 1;
        case INTERPRET_RUNTIME_ERROR:
            printf("错误: 运行时错误\n");
            return 2;
        default:
            printf("错误: 未知错误\n");
            return 3;
    }
}