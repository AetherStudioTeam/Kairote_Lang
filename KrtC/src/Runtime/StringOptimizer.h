#ifndef KRT_STRING_OPTIMIZER_H
#define KRT_STRING_OPTIMIZER_H

#include <stddef.h>

const char* KrtGetStringConstant(const char* str);

char* KrtStrcatOptimized(const char* str1, const char* str2);
char* KrtStrcatMultiple(const char** parts, int count);

char* KrtIntToStringOptimized(int num);
char* KrtDoubleToStringOptimized(double num);

int KrtStringFastCompare(const char* str1, const char* str2);

void KrtProcessStringsBatch(const char** strings, int count, void (*processor)(const char*));

char* KrtPoolAllocString(size_t size);

#endif 