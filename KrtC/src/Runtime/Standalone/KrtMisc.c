#include "KrtStandalone.h"

int KrtAtoi(const char* nptr) {
    return (int)KrtStrtol(nptr, NULL, 10);
}

long KrtAtol(const char* nptr) {
    return KrtStrtol(nptr, NULL, 10);
}

long long KrtAtoll(const char* nptr) {
    long long result = 0;
    int sign = 1;
    
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || 
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
        nptr++;
    }
    
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    
    return sign * result;
}

long KrtStrtol(const char* nptr, char** endptr, int base) {
    const char* p = nptr;
    long result = 0;
    int sign = 1;
    int digit;
    
    while (*p == ' ' || *p == '\t' || *p == '\n' || 
           *p == '\r' || *p == '\f' || *p == '\v') {
        p++;
    }
    
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    if (base == 0) {
        if (*p == '0') {
            if (p[1] == 'x' || p[1] == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    
    while (1) {
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) break;
        
        result = result * base + digit;
        p++;
    }
    
    if (endptr) {
        *endptr = (char*)p;
    }
    
    return sign * result;
}

unsigned long KrtStrtoul(const char* nptr, char** endptr, int base) {
    const char* p = nptr;
    unsigned long result = 0;
    int digit;
    
    while (*p == ' ' || *p == '\t' || *p == '\n' || 
           *p == '\r' || *p == '\f' || *p == '\v') {
        p++;
    }
    
    if (*p == '+' || *p == '-') {
        p++;
    }
    
    if (base == 0) {
        if (*p == '0') {
            if (p[1] == 'x' || p[1] == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    
    while (1) {
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) break;
        
        result = result * base + digit;
        p++;
    }
    
    if (endptr) {
        *endptr = (char*)p;
    }
    
    return result;
}

static unsigned int g_rand_seed = 1;

int KrtRand(void) {
    g_rand_seed = g_rand_seed * 1103515245 + 12345;
    return (int)((g_rand_seed / 65536) % 32768);
}

void KrtSrand(unsigned int seed) {
    g_rand_seed = seed;
}

static void swap(void* a, void* b, size_t size) {
    unsigned char* pa = (unsigned char*)a;
    unsigned char* pb = (unsigned char*)b;
    unsigned char temp;
    
    while (size--) {
        temp = *pa;
        *pa++ = *pb;
        *pb++ = temp;
    }
}

static void quicksort(void* base, size_t nmemb, size_t size,
                      int (*compar)(const void*, const void*),
                      int left, int right) {
    if (left >= right) return;
    
    char* arr = (char*)base;
    int i = left;
    int j = right;
    void* pivot = arr + ((left + right) / 2) * size;
    
    while (i <= j) {
        while (compar(arr + i * size, pivot) < 0) i++;
        while (compar(arr + j * size, pivot) > 0) j--;
        
        if (i <= j) {
            swap(arr + i * size, arr + j * size, size);
            i++;
            j--;
        }
    }
    
    if (left < j) quicksort(base, nmemb, size, compar, left, j);
    if (i < right) quicksort(base, nmemb, size, compar, i, right);
}

void KrtQsort(void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
    if (nmemb <= 1) return;
    quicksort(base, nmemb, size, compar, 0, (int)nmemb - 1);
}

void* KrtBsearch(const void* key, const void* base, size_t nmemb, size_t size,
                 int (*compar)(const void*, const void*)) {
    const char* arr = (const char*)base;
    size_t left = 0;
    size_t right = nmemb;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        const void* mid_elem = arr + mid * size;
        int cmp = compar(key, mid_elem);
        
        if (cmp == 0) {
            return (void*)mid_elem;
        } else if (cmp < 0) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    return NULL;
}

#ifdef _WIN32
#include <windows.h>

KrtClockT KrtClock(void) {
    LARGE_INTEGER freq, count;
    if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
        
        return (KrtClockT)(count.QuadPart * 1000 / freq.QuadPart);
    }
    return 0;
}

KrtTimeT KrtTime(KrtTimeT* tloc) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    
    KrtTimeT t = (KrtTimeT)(ull.QuadPart / 10000000ULL - 11644473600ULL);
    
    if (tloc) {
        *tloc = t;
    }
    
    return t;
}

#else

#include <sys/time.h>
#include <time.h>

KrtClockT KrtClock(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (KrtClockT)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
    return 0;
}

KrtTimeT KrtTime(KrtTimeT* tloc) {
    KrtTimeT t = (KrtTimeT)time(NULL);
    if (tloc) {
        *tloc = t;
    }
    return t;
}

#endif