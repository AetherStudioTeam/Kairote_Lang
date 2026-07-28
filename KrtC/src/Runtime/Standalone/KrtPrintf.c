#include "KrtStandalone.h"
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

static char* itoa_internal(long long value, char* str, int base, int uppercase) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    long long tmp_value;
    
    if (value < 0 && base == 10) {
        *ptr++ = '-';
    }
    
    if (value == 0) {
        *ptr++ = '0';
    }
    
    tmp_value = value < 0 ? -value : value;
    while (tmp_value != 0) {
        int digit = tmp_value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : 
                 (uppercase ? ('A' + digit - 10) : ('a' + digit - 10));
        tmp_value /= base;
    }
    
    *ptr-- = '\0';
    
    if (str[0] == '-') ptr1++;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

static char* utoa_internal(unsigned long long value, char* str, int base, int uppercase) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    
    if (value == 0) {
        *ptr++ = '0';
    }
    
    while (value != 0) {
        unsigned int digit = value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : 
                 (uppercase ? ('A' + digit - 10) : ('a' + digit - 10));
        value /= base;
    }
    
    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

static int output_string(char* buf, size_t size, size_t* pos, const char* str) {
    int count = 0;
    while (*str) {
        if (*pos < size - 1) {
            buf[(*pos)++] = *str;
        }
        str++;
        count++;
    }
    return count;
}

static int output_char(char* buf, size_t size, size_t* pos, char c) {
    if (*pos < size - 1) {
        buf[(*pos)++] = c;
    }
    return 1;
}

static int output_pad(char* buf, size_t size, size_t* pos, char pad_char, int count) {
    int n = 0;
    for (int i = 0; i < count; i++) {
        n += output_char(buf, size, pos, pad_char);
    }
    return n;
}

int KrtVsnprintf(char* str, size_t size, const char* format, va_list ap) {
    if (!format) return -1;
    if (size == 0) return 0;
    
    size_t pos = 0;
    const char* p = format;
    char num_buf[64];
    
    while (*p && pos < size - 1) {
        if (*p != '%') {
            str[pos++] = *p++;
            continue;
        }
        
        p++;  
        
        int width = 0;
        int precision = -1;
        int left_align = 0;
        int zero_pad = 0;
        int long_flag = 0;
        int long_long_flag = 0;

        
        while (*p) {
            if (*p == '-') left_align = 1;
            else if (*p == '0') zero_pad = 1;
            else if (*p == '+') ;  
            else if (*p == ' ') ;  
            else if (*p == '#') ;  
            else break;
            p++;
        }
        
        if (*p >= '0' && *p <= '9') {
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }
        
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') {
                precision = precision * 10 + (*p - '0');
                p++;
            }
        }
        
        if (*p == 'l') {
            p++;
            if (*p == 'l') {
                long_long_flag = 1;
                p++;
            } else {
                long_flag = 1;
            }
        } else if (*p == 'h') {
            p++;
            if (*p == 'h') p++;
        } else if (*p == 'z') {
            p++;
        }
        
        switch (*p) {
            case 'd':
            case 'i': {
                long long val;
                if (long_long_flag) val = va_arg(ap, long long);
                else if (long_flag) val = va_arg(ap, long);
                else val = va_arg(ap, int);
                
                itoa_internal(val, num_buf, 10, 0);
                int len = KrtStrlen(num_buf);
                int pad = width > len ? width - len : 0;
                
                if (!left_align && pad > 0) {
                    output_pad(str, size, &pos, zero_pad ? '0' : ' ', pad);
                }
                output_string(str, size, &pos, num_buf);
                if (left_align && pad > 0) {
                    output_pad(str, size, &pos, ' ', pad);
                }
                break;
            }
            
            case 'u': {
                unsigned long long val;
                if (long_long_flag) val = va_arg(ap, unsigned long long);
                else if (long_flag) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                
                utoa_internal(val, num_buf, 10, 0);
                int len = KrtStrlen(num_buf);
                int pad = width > len ? width - len : 0;
                
                if (!left_align && pad > 0) {
                    output_pad(str, size, &pos, zero_pad ? '0' : ' ', pad);
                }
                output_string(str, size, &pos, num_buf);
                if (left_align && pad > 0) {
                    output_pad(str, size, &pos, ' ', pad);
                }
                break;
            }
            
            case 'x':
            case 'X': {
                unsigned long long val;
                if (long_long_flag) val = va_arg(ap, unsigned long long);
                else if (long_flag) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                
                utoa_internal(val, num_buf, 16, *p == 'X');
                int len = KrtStrlen(num_buf);
                int pad = width > len ? width - len : 0;
                
                if (!left_align && pad > 0) {
                    output_pad(str, size, &pos, zero_pad ? '0' : ' ', pad);
                }
                output_string(str, size, &pos, num_buf);
                if (left_align && pad > 0) {
                    output_pad(str, size, &pos, ' ', pad);
                }
                break;
            }
            
            case 'p': {
                void* val = va_arg(ap, void*);
                output_string(str, size, &pos, "0x");
                utoa_internal((unsigned long long)val, num_buf, 16, 0);
                output_string(str, size, &pos, num_buf);
                break;
            }
            
            case 'c': {
                char val = (char)va_arg(ap, int);
                if (!left_align && width > 1) {
                    output_pad(str, size, &pos, ' ', width - 1);
                }
                output_char(str, size, &pos, val);
                if (left_align && width > 1) {
                    output_pad(str, size, &pos, ' ', width - 1);
                }
                break;
            }
            
            case 'f':
            case 'F': {
                double val = va_arg(ap, double);
                
                int prec = (precision >= 0) ? precision : 6;
                
                if (val < 0) {
                    output_char(str, size, &pos, '-');
                    val = -val;
                }
                
                long long int_part = (long long)val;
                double frac_part = val - int_part;
                
                itoa_internal(int_part, num_buf, 10, 0);
                int len = KrtStrlen(num_buf);
                
                if (!left_align && width > len + 1 + prec) {
                    output_pad(str, size, &pos, zero_pad ? '0' : ' ', width - len - 1 - prec);
                }
                
                output_string(str, size, &pos, num_buf);
                output_char(str, size, &pos, '.');
                
                for (int i = 0; i < prec; i++) {
                    frac_part *= 10;
                    int digit = (int)frac_part;
                    output_char(str, size, &pos, '0' + digit);
                    frac_part -= digit;
                }
                
                if (left_align && width > len + 1 + prec) {
                    output_pad(str, size, &pos, ' ', width - len - 1 - prec);
                }
                break;
            }
            
            case 's': {
                const char* val = va_arg(ap, const char*);
                if (!val) val = "(null)";
                
                int len = KrtStrlen(val);
                if (precision >= 0 && len > precision) {
                    len = precision;
                }
                int pad = width > len ? width - len : 0;
                
                if (!left_align && pad > 0) {
                    output_pad(str, size, &pos, ' ', pad);
                }
                
                for (int i = 0; i < len && pos < size - 1; i++) {
                    str[pos++] = val[i];
                }
                
                if (left_align && pad > 0) {
                    output_pad(str, size, &pos, ' ', pad);
                }
                break;
            }
            
            case '%': {
                output_char(str, size, &pos, '%');
                break;
            }
            
            default:
                
                output_char(str, size, &pos, '%');
                if (*p) {
                    output_char(str, size, &pos, *p);
                }
                break;
        }
        
        if (*p) p++;
    }
    
    str[pos] = '\0';
    return (int)pos;
}

int KrtSnprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = KrtVsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int KrtVsprintf(char* str, const char* format, va_list ap) {
    return KrtVsnprintf(str, (size_t)-1, format, ap);
}

int KrtSprintf(char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = KrtVsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

#ifdef _WIN32

static void* get_stdout_handle(void) {
    return GetStdHandle(STD_OUTPUT_HANDLE);
}



static int write_console(const char* buffer, size_t size) {
    void* handle = get_stdout_handle();
    if (handle == INVALID_HANDLE_VALUE) return -1;
    
    DWORD written;
    
    if (WriteConsoleA(handle, buffer, (DWORD)size, &written, NULL)) {
        return (int)written;
    }
    
    if (WriteFile(handle, buffer, (DWORD)size, &written, NULL)) {
        return (int)written;
    }
    
    return -1;
}

#else

#include <unistd.h>

static int write_console(const char* buffer, size_t size) {
    ssize_t ret = write(STDOUT_FILENO, buffer, size);
    return (ret < 0) ? -1 : (int)ret;
}

#endif

int KrtVprintf(const char* format, va_list ap) {
    char buffer[4096];
    int ret = KrtVsnprintf(buffer, sizeof(buffer), format, ap);
    if (ret > 0) {
        write_console(buffer, ret);
    }
    return ret;
}

int KrtPrintf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = KrtVprintf(format, ap);
    va_end(ap);
    return ret;
}

int KrtPutchar(int c) {
    char ch = (char)c;
    write_console(&ch, 1);
    return c;
}

int KrtPuts(const char* s) {
    if (!s) return -1;
    int len = KrtStrlen(s);
    write_console(s, len);
    write_console("\n", 1);
    return len + 1;
}