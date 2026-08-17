#ifndef KRT_PREPROCESSOR_H
#define KRT_PREPROCESSOR_H

#include "../../Core/Utils/KrtCommon.h"

typedef struct {
    char* name;
    char* replacement;
} Macro;

typedef struct {
    Macro* macros;
    int macro_count;
    int macro_capacity;
} Preprocessor;

Preprocessor* PreprocessorCreate(void);
void PreprocessorDestroy(Preprocessor* preprocessor);
bool PreprocessorAddMacro(Preprocessor* preprocessor, const char* name, const char* replacement);
char* PreprocessorProcess(Preprocessor* preprocessor, const char* source);

#endif
