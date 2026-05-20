// Shared Unity entry point. Each test_<name>.c defines its own RunTests()
// that calls RUN_TEST on its cases — the runner just bookends with setup
// and tear-down boilerplate.
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void RunTests(void);

int main(void)
{
    UNITY_BEGIN();
    RunTests();
    return UNITY_END();
}
