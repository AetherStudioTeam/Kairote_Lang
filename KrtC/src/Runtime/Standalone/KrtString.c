#include "KrtStandalone.h"

void* KrtMemcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

void* KrtMemset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char value = (unsigned char)c;
    
    while (n--) {
        *p++ = value;
    }
    
    return s;
}

void* KrtMemmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d > s && d < s + n) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    } else {
        
        while (n--) {
            *d++ = *s++;
        }
    }
    
    return dest;
}

int KrtMemcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

void* KrtMemchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char value = (unsigned char)c;
    
    while (n--) {
        if (*p == value) {
            return (void*)p;
        }
        p++;
    }
    
    return NULL;
}

size_t KrtStrlen(const char* s) {
    const char* p = s;
    while (*p) {
        p++;
    }
    return p - s;
}

char* KrtStrcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* KrtStrncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    
    while (n && (*d++ = *src++)) {
        n--;
    }
    
    while (n--) {
        *d++ = '\0';
    }
    
    return dest;
}

int KrtStrcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int KrtStrncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* KrtStrcat(char* dest, const char* src) {
    char* d = dest;
    
    while (*d) {
        d++;
    }
    
    while ((*d++ = *src++));
    
    return dest;
}

char* KrtStrncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    
    while (*d) {
        d++;
    }
    
    while (n-- && (*d++ = *src++));
    
    *d = '\0';
    
    return dest;
}

char* KrtStrchr(const char* s, int c) {
    char ch = (char)c;
    
    while (*s) {
        if (*s == ch) {
            return (char*)s;
        }
        s++;
    }
    
    if (ch == '\0') {
        return (char*)s;
    }
    
    return NULL;
}

char* KrtStrrchr(const char* s, int c) {
    char ch = (char)c;
    const char* last = NULL;
    
    while (*s) {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    
    if (ch == '\0') {
        return (char*)s;
    }
    
    return (char*)last;
}

char* KrtStrstr(const char* haystack, const char* needle) {
    if (!*needle) {
        return (char*)haystack;
    }
    
    const char* h = haystack;
    
    while (*h) {
        const char* h2 = h;
        const char* n = needle;
        
        while (*h2 && *n && *h2 == *n) {
            h2++;
            n++;
        }
        
        if (!*n) {
            return (char*)h;
        }
        
        h++;
    }
    
    return NULL;
}