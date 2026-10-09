/* Khung test tối giản cho PC. */
#ifndef TESTLIB_H
#define TESTLIB_H
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern int t_fail, t_pass;
extern const char *t_name;

#define CHECK(c) do { if (c) t_pass++; else { t_fail++; printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, t_name, #c); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); \
    if (_a == _b) t_pass++; else { t_fail++; printf("  FAIL %s:%d [%s] %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, t_name, #a, #b, _a, _b); } } while (0)
#define CHECK_MEM(a, b, n) do { if (memcmp((a), (b), (n)) == 0) t_pass++; else { t_fail++; printf("  FAIL %s:%d [%s] memcmp %s %s\n", __FILE__, __LINE__, t_name, #a, #b); } } while (0)
#define RUN(fn) do { t_name = #fn; int _f = t_fail; fn(); printf("%-40s %s\n", #fn, t_fail == _f ? "ok" : "FAILED"); } while (0)

#endif
