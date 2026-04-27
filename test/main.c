// Host test runner.  Each TEST() macro registers itself via a constructor.
// main() simply walks the registration table.

#include <stdio.h>
#include "test_common.h"


test_case_t g_tests[TEST_MAX];
int         g_tests_n;


void test_register(const char *name, int (*fn)(void))
{
    if(g_tests_n < TEST_MAX) 
    {
        g_tests[g_tests_n].name = name;
        g_tests[g_tests_n].fn   = fn;
        g_tests_n++;
    }
}

int main(void)
{
    int passed = 0, failed = 0;
    for(int i = 0; i < g_tests_n; ++i) 
    {
        fprintf(stderr, "RUN  %s\n", g_tests[i].name);
        int rc = g_tests[i].fn();
        if(rc == 0) 
        {
            fprintf(stderr, "OK   %s\n", g_tests[i].name); 
            passed++; 
        }
        else         
        {
            fprintf(stderr, "FAIL %s\n", g_tests[i].name); 
            failed++; 
        }
    }

    fprintf(stderr, "\n%d passed, %d failed (of %d)\n",
            passed, failed, g_tests_n);
    
    return failed == 0 ? 0 : 1;
}
