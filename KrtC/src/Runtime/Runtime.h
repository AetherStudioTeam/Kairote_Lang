#ifndef KRT_RUNTIME_H
#define KRT_RUNTIME_H

#include "../Core/Platform/Platform.h"
#include "../Core/Utils/KrtCommon.h"
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
    #define KRT_RUNTIME_EXPORT __declspec(dllexport)
#else
    
    #define KRT_RUNTIME_EXPORT 
#endif

typedef struct {
    int x;
    int y;
} KrtPoint;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} KrtRect;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} KrtColor;

typedef struct {
    void* data;
    size_t size;
    size_t capacity;
    size_t element_size;
} KrtArray;

typedef enum {
    KRT_HASHMAP_TYPE_INT,
    KRT_HASHMAP_TYPE_DOUBLE,
    KRT_HASHMAP_TYPE_STRING,
    KRT_HASHMAP_TYPE_POINTER
} KrtHashMapType;

typedef struct KrtHashMapItem {
    void* key;
    void* value;
    KrtHashMapType key_type;
    KrtHashMapType value_type;
    struct KrtHashMapItem* next;
} KrtHashMapItem;

typedef struct {
    KrtHashMapItem** buckets;
    size_t capacity;
    size_t size;
} KrtHashMap;

typedef void* KrtMutex;
typedef void* KrtWindow;
typedef void* KrtFile;
typedef void* KrtSocketHandle;
typedef void* KrtThread;

typedef enum {
    KRT_ERR_NONE = 0,
    KRT_ERR_OUT_OF_MEMORY,
    KRT_ERR_INVALID_ARGUMENT,
    KRT_ERR_FILE_NOT_FOUND,
    KRT_ERR_PERMISSION_DENIED,
    KRT_ERR_IO_ERROR,
    KRT_ERR_NETWORK_ERROR,
    KRT_ERR_INDEX_OUT_OF_BOUNDS,
    KRT_ERR_DIVISION_BY_ZERO,
    KRT_ERR_NULL_POINTER,
    KRT_ERR_BUFFER_OVERFLOW,
    KRT_ERR_UNKNOWN
} KrtErrorCode;

KRT_RUNTIME_EXPORT void KRT_API _print_string(const char* str);
KRT_RUNTIME_EXPORT void KRT_API _print_number(int num);
KRT_RUNTIME_EXPORT void KRT_API _print_int(int num);
KRT_RUNTIME_EXPORT void KRT_API _print_int64(long long num);
KRT_RUNTIME_EXPORT void KRT_API _print_double(double num);
KRT_RUNTIME_EXPORT void KRT_API _print_float(double num);
KRT_RUNTIME_EXPORT void KRT_API _print_char(char c);
KRT_RUNTIME_EXPORT void KRT_API _print_newline(void);
KRT_RUNTIME_EXPORT int KRT_API _read_char(void);
KRT_RUNTIME_EXPORT int KRT_API _pow_int(int base, int exp);
KRT_RUNTIME_EXPORT void* KRT_API KrtMalloc(size_t size);
KRT_RUNTIME_EXPORT void KRT_API KrtFree(void* ptr);
KRT_RUNTIME_EXPORT void* KRT_API KrtRealloc(void* ptr, size_t size);
KRT_RUNTIME_EXPORT void* KRT_API KrtCalloc(size_t num, size_t size);
KRT_RUNTIME_EXPORT uint64_t KRT_API get_total_memory(void);
KRT_RUNTIME_EXPORT uint64_t KRT_API get_free_memory(void);
KRT_RUNTIME_EXPORT size_t KRT_API KrtStrlen(const char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrcpy(char* dest, const char* src);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrcat(char* dest, const char* src);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrdup(const char* src);
KRT_RUNTIME_EXPORT int KRT_API KrtStrcmp(const char* str1, const char* str2);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrchr(const char* str, int c);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrstr(const char* haystack, const char* needle);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrtok(char* str, const char* delim);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrupr(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrlwr(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrrev(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrtrim(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrltrim(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrrtrim(char* str);
KRT_RUNTIME_EXPORT char* KRT_API KrtStrreplace(const char* src, const char* old_sub, const char* new_sub);
KRT_RUNTIME_EXPORT int KRT_API KrtAtoi(const char* str);
KRT_RUNTIME_EXPORT double KRT_API KrtAtof(const char* str);
KRT_RUNTIME_EXPORT char* KRT_API _KrtIntToString_value(int64_t num);
KRT_RUNTIME_EXPORT char* KRT_API KrtIntToString(int64_t num);
KRT_RUNTIME_EXPORT char* KRT_API _KrtDoubleToStringValue(double num);
KRT_RUNTIME_EXPORT char* KRT_API _KrtStrcat(const char* str1, const char* str2);
KRT_RUNTIME_EXPORT double KRT_API KrtSin(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtCos(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtTan(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtSqrt(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtPow(double base, double exp);
KRT_RUNTIME_EXPORT double KRT_API KrtLog(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtLog10(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtExp(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtAsin(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtAcos(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtAtan(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtSinh(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtCosh(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtTanh(double x);
KRT_RUNTIME_EXPORT double KRT_API KrtFabs(double x);
KRT_RUNTIME_EXPORT int KRT_API KrtAbs(int x);
KRT_RUNTIME_EXPORT double KRT_API KrtTime(void);
KRT_RUNTIME_EXPORT void KRT_API KrtSleep(int seconds);
KRT_RUNTIME_EXPORT void KRT_API KrtSrand(unsigned int seed);
KRT_RUNTIME_EXPORT int KRT_API KrtRand(void);
KRT_RUNTIME_EXPORT char* KRT_API KrtDate(void);
KRT_RUNTIME_EXPORT char* KRT_API KrtTimeFormat(const char* format);
KRT_RUNTIME_EXPORT double KRT_API timer_start(void);
KRT_RUNTIME_EXPORT double KRT_API timer_elapsed(void);
KRT_RUNTIME_EXPORT double KRT_API timer_current(void);
KRT_RUNTIME_EXPORT long long KRT_API timer_start_int(void);
KRT_RUNTIME_EXPORT long long KRT_API timer_elapsed_int(void);
KRT_RUNTIME_EXPORT long long KRT_API timer_current_int(void);
KRT_RUNTIME_EXPORT void KRT_API KrtPrint(const char* str);
KRT_RUNTIME_EXPORT void KRT_API KrtPrintln(const char* str);
KRT_RUNTIME_EXPORT void KRT_API Console__WriteLine(const char* str);
KRT_RUNTIME_EXPORT void KRT_API Console__Write(const char* str);
KRT_RUNTIME_EXPORT void KRT_API Console__WriteLineInt(int num);
KRT_RUNTIME_EXPORT void KRT_API Console__WriteInt(int num);
KRT_RUNTIME_EXPORT KrtFile KRT_API KrtFopen(const char* filename, const char* mode);
KRT_RUNTIME_EXPORT int KRT_API KrtFclose(KrtFile file);
KRT_RUNTIME_EXPORT size_t KRT_API KrtFread(void* buffer, size_t size, size_t count, KrtFile file);
KRT_RUNTIME_EXPORT size_t KRT_API KrtFwrite(const void* buffer, size_t size, size_t count, KrtFile file);
KRT_RUNTIME_EXPORT int KRT_API KrtFseek(KrtFile file, long offset, int origin);
KRT_RUNTIME_EXPORT long KRT_API KrtFtell(KrtFile file);
KRT_RUNTIME_EXPORT int KRT_API KrtFeof(KrtFile file);
KRT_RUNTIME_EXPORT int KRT_API KrtRemove(const char* filename);
KRT_RUNTIME_EXPORT int KRT_API KrtRename(const char* oldname, const char* newname);
KRT_RUNTIME_EXPORT const char* KRT_API KrtStrerror(KrtErrorCode code);
KRT_RUNTIME_EXPORT KrtArray* KRT_API array_create(size_t element_size, size_t initial_capacity);
KRT_RUNTIME_EXPORT void KRT_API array_KRT_FREE(KrtArray* array);
KRT_RUNTIME_EXPORT size_t KRT_API array_size(KrtArray* array);
KRT_RUNTIME_EXPORT size_t KRT_API array_capacity(KrtArray* array);
KRT_RUNTIME_EXPORT int KRT_API array_resize(KrtArray* array, size_t new_capacity);
KRT_RUNTIME_EXPORT int KRT_API array_append(KrtArray* array, const void* element);
KRT_RUNTIME_EXPORT void* KRT_API array_get(KrtArray* array, size_t index);
KRT_RUNTIME_EXPORT int KRT_API array_set(KrtArray* array, size_t index, const void* element);
KRT_RUNTIME_EXPORT KrtHashMap* KRT_API hashmap_create(size_t initial_capacity);
KRT_RUNTIME_EXPORT void KRT_API hashmap_KRT_FREE(KrtHashMap* map);
KRT_RUNTIME_EXPORT int KRT_API hashmap_put(KrtHashMap* map, const void* key, KrtHashMapType key_type, const void* value, KrtHashMapType value_type);
KRT_RUNTIME_EXPORT int KRT_API hashmap_get(KrtHashMap* map, const void* key, KrtHashMapType key_type, void** value, KrtHashMapType* out_value_type);
KRT_RUNTIME_EXPORT int KRT_API hashmap_remove(KrtHashMap* map, const void* key, KrtHashMapType key_type);
KRT_RUNTIME_EXPORT KrtSocketHandle KRT_API KrtSocketCreate(int domain, int type, int protocol);
KRT_RUNTIME_EXPORT int KRT_API KrtConnect(KrtSocketHandle socket, const char* address, int port);
KRT_RUNTIME_EXPORT int KRT_API KrtBind(KrtSocketHandle socket, const char* address, int port);
KRT_RUNTIME_EXPORT int KRT_API KrtListen(KrtSocketHandle socket, int backlog);
KRT_RUNTIME_EXPORT KrtSocketHandle KRT_API KrtAccept(KrtSocketHandle socket);
KRT_RUNTIME_EXPORT int KRT_API KrtSend(KrtSocketHandle socket, const char* data, size_t length);
KRT_RUNTIME_EXPORT int KRT_API KrtRecv(KrtSocketHandle socket, char* buffer, size_t length);
KRT_RUNTIME_EXPORT int KRT_API KrtCloseSocket(KrtSocketHandle socket);
KRT_RUNTIME_EXPORT KrtThread KRT_API KrtThreadCreateFunc(void (*func)(void*), void* arg);
KRT_RUNTIME_EXPORT int KRT_API KrtThreadJoinFunc(KrtThread thread);
KRT_RUNTIME_EXPORT KrtMutex KRT_API KrtMutexCreateFunc(void);
KRT_RUNTIME_EXPORT int KRT_API KrtMutexLockFunc(KrtMutex mutex);
KRT_RUNTIME_EXPORT int KRT_API KrtMutexUnlockFunc(KrtMutex mutex);
KRT_RUNTIME_EXPORT void KRT_API KrtMutexFreeFunc(KrtMutex mutex);
KRT_RUNTIME_EXPORT KrtWindow KRT_API KrtWindowCreate(int width, int height, const char* title);
KRT_RUNTIME_EXPORT void KRT_API KrtWindowShow(KrtWindow window);
KRT_RUNTIME_EXPORT void KRT_API KrtWindowClose(KrtWindow window);
KRT_RUNTIME_EXPORT void KRT_API KrtWindowClear(KrtWindow window, KrtColor color);
KRT_RUNTIME_EXPORT void KRT_API KrtWindowUpdate(KrtWindow window);
KRT_RUNTIME_EXPORT int KRT_API KrtWindowIsOpen(KrtWindow window);
KRT_RUNTIME_EXPORT void KRT_API KrtDrawRect(KrtWindow window, KrtRect rect, KrtColor color);
KRT_RUNTIME_EXPORT void KRT_API KrtDrawCircle(KrtWindow window, KrtPoint center, int radius, KrtColor color);
KRT_RUNTIME_EXPORT void KRT_API KrtDrawText(KrtWindow window, const char* text, KrtPoint position, KrtColor color);
KRT_RUNTIME_EXPORT void KRT_API KrtExit(int code);

KRT_RUNTIME_EXPORT int KRT_API KrtIsInstance(void* obj, const char* typeName);
KRT_RUNTIME_EXPORT void* KRT_API KrtAsInstance(void* obj, const char* typeName);
KRT_RUNTIME_EXPORT void* KRT_API KrtNullCoalesce(void* left, void* right);
KRT_RUNTIME_EXPORT void* KRT_API KrtNullConditional(void* obj, const char* memberName);
KRT_RUNTIME_EXPORT void KRT_API Monitor_Enter(void* obj);
KRT_RUNTIME_EXPORT void KRT_API Monitor_Exit(void* obj);
KRT_RUNTIME_EXPORT void KRT_API KrtThrowException(void* exception);
KRT_RUNTIME_EXPORT void KRT_API KrtRethrowException(void);
KRT_RUNTIME_EXPORT void* KRT_API KrtGetCurrentException(void);
KRT_RUNTIME_EXPORT void KRT_API KrtSetCurrentException(void* exception);
KRT_RUNTIME_EXPORT void KRT_API Dispose(void* obj);

// Type casting functions
KRT_RUNTIME_EXPORT int KRT_API KrtCastToInt32(double value);
KRT_RUNTIME_EXPORT long long KRT_API KrtCastToInt64(double value);
KRT_RUNTIME_EXPORT float KRT_API KrtCastToFloat32(double value);
KRT_RUNTIME_EXPORT double KRT_API KrtCastToFloat64(double value);
KRT_RUNTIME_EXPORT int KRT_API KrtCastToBool(double value);
KRT_RUNTIME_EXPORT char* KRT_API KrtCastToString(double value);

// LINQ OrderBy support
typedef int (KRT_API *KrtCompareFunc)(const void*, const void*);
KRT_RUNTIME_EXPORT void KRT_API KrtOrderBy(void* array, int count, size_t element_size, KrtCompareFunc compare);
KRT_RUNTIME_EXPORT int KRT_API KrtCompareInt32Asc(const void* a, const void* b);
KRT_RUNTIME_EXPORT int KRT_API KrtCompareInt32Desc(const void* a, const void* b);
KRT_RUNTIME_EXPORT int KRT_API KrtCompareFloat64Asc(const void* a, const void* b);
KRT_RUNTIME_EXPORT int KRT_API KrtCompareFloat64Desc(const void* a, const void* b);

// sizeof support
KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfInt32(void);
KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfInt64(void);
KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfFloat32(void);
KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfFloat64(void);
KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfPointer(void);

// String interpolation support
KRT_RUNTIME_EXPORT char* KRT_API KrtStringConcat(const char* s1, const char* s2);
KRT_RUNTIME_EXPORT char* KRT_API KrtInt32ToString(int32_t value);
KRT_RUNTIME_EXPORT char* KRT_API KrtInt64ToString(int64_t value);
KRT_RUNTIME_EXPORT char* KRT_API KrtFloat32ToString(float value);
KRT_RUNTIME_EXPORT char* KRT_API KrtFloat64ToString(double value);
KRT_RUNTIME_EXPORT char* KRT_API KrtBoolToString(int value);
KRT_RUNTIME_EXPORT char* KRT_API KrtPointerToString(void* value);

// Tuple support
KRT_RUNTIME_EXPORT void* KRT_API KrtCreateTuple(int element_count);
KRT_RUNTIME_EXPORT void KRT_API KrtTupleSetElement(void* tuple, int index, void* value);
KRT_RUNTIME_EXPORT void* KRT_API KrtTupleGetElement(void* tuple, int index);

// Pointer and Memory Operations
KRT_RUNTIME_EXPORT void* KRT_API KrtGetVariableAddress(const char* var_name);
KRT_RUNTIME_EXPORT void* KRT_API KrtLoadPtr(void* base, int offset);
KRT_RUNTIME_EXPORT void KRT_API KrtStorePtr(void* base, int offset, void* value);
KRT_RUNTIME_EXPORT void* KRT_API KrtStackAlloc(int size);
KRT_RUNTIME_EXPORT void KRT_API KrtPinObject(void* obj);
KRT_RUNTIME_EXPORT void KRT_API KrtUnpinObject(void* obj);

// Async/Await Support
KRT_RUNTIME_EXPORT void* KRT_API KrtCreateTask(void);
KRT_RUNTIME_EXPORT void KRT_API KrtCompleteTask(void* task_handle, void* result);
KRT_RUNTIME_EXPORT void* KRT_API KrtAwaitTask(void* task_handle);
KRT_RUNTIME_EXPORT int KRT_API KrtTaskIsCompleted(void* task_handle);

// LINQ Support Functions
KRT_RUNTIME_EXPORT void* KRT_API KrtLinqWhere(void* source, void* predicate);
KRT_RUNTIME_EXPORT void* KRT_API KrtLinqOrderBy(void* source, int ascending, void* key_selector);
KRT_RUNTIME_EXPORT void* KRT_API KrtLinqGroupBy(void* source, void* key, void* element, const char* into_var);
KRT_RUNTIME_EXPORT void* KRT_API KrtLinqSelect(void* source, void* selector);

// Delegate Support
KRT_RUNTIME_EXPORT void* KRT_API KrtCreateDelegate(void* target, void* method_ptr, const char* method_name);
KRT_RUNTIME_EXPORT void* KRT_API KrtInvokeDelegate(void* delegate_handle, void** args, int arg_count);
KRT_RUNTIME_EXPORT void KRT_API KrtFreeDelegate(void* delegate_handle);

// Generic Static Field Support
KRT_RUNTIME_EXPORT void* KRT_API KrtGetGenericStaticField(const char* mangled_name);
KRT_RUNTIME_EXPORT void KRT_API KrtSetGenericStaticField(const char* mangled_name, void* value);
KRT_RUNTIME_EXPORT void KRT_API KrtClearGenericStaticFields(void);

// Generic Constraint Support
KRT_RUNTIME_EXPORT int KRT_API KrtIsClass(void* obj);
KRT_RUNTIME_EXPORT int KRT_API KrtIsStruct(void* obj);
KRT_RUNTIME_EXPORT int KRT_API KrtImplementsInterface(void* obj, const char* interface_name);
KRT_RUNTIME_EXPORT int KRT_API KrtInheritsFrom(void* obj, const char* base_class_name);
KRT_RUNTIME_EXPORT int KRT_API KrtCheckGenericConstraint(void* obj, const char* constraint_type);

#endif 