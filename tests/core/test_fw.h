#ifndef TEST_FW_H
#define TEST_FW_H

/* ============================================================================
 * 极简 C 单元测试框架（core 层 PC 单测用，零第三方依赖）
 * 用法：
 *   TEST_GROUP(test_rb);            // 注册测试组（自动生成 main）
 *   TEST_CASE(rb_basic_write_read) { ... TEST_ASSERT_EQ(...); ... }
 * 编译：tools/run_core_tests.sh
 * ==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 断言：失败打印文件:行并计数（不中断，便于一次跑完所有用例） */
#define TEST_ASSERT(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            test_fail_count++;                                                                                         \
            printf("  ✗ %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                      \
        }                                                                                                              \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected)                                                                               \
    do {                                                                                                               \
        long long _a = (long long)(actual);                                                                            \
        long long _e = (long long)(expected);                                                                          \
        if (_a != _e) {                                                                                                \
            test_fail_count++;                                                                                         \
            printf("  ✗ %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #actual, #expected, _a, _e);            \
        }                                                                                                              \
    } while (0)

#define TEST_ASSERT_NE(actual, expected)                                                                               \
    do {                                                                                                               \
        long long _a = (long long)(actual);                                                                            \
        long long _e = (long long)(expected);                                                                          \
        if (_a == _e) {                                                                                                \
            test_fail_count++;                                                                                         \
            printf("  ✗ %s:%d: %s != %s (%lld == %lld)\n", __FILE__, __LINE__, #actual, #expected, _a, _e);            \
        }                                                                                                              \
    } while (0)

#define TEST_ASSERT_MEM_EQ(actual, expected, len)                                                                      \
    do {                                                                                                               \
        if (memcmp((actual), (expected), (len)) != 0) {                                                                \
            test_fail_count++;                                                                                         \
            printf("  ✗ %s:%d: memcmp(%s, %s, %d)\n", __FILE__, __LINE__, #actual, #expected, (int)(len));             \
        }                                                                                                              \
    } while (0)

/* 用例/组注册：组为纯标记（避免 -Werror 未使用函数告警）；用例展开为 static 函数 */
#define TEST_GROUP(name)
#define TEST_CASE(name) static void test_case_##name(void)

/* 全局计数（由框架 main 维护） */
static int test_fail_count;

#endif /* TEST_FW_H */
