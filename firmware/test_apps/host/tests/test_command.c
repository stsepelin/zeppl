#include "unity.h"
#include "command.h"

// The command-dispatch seam (contract/command.c): connectivity emits typed
// commands, the composition root registers one handler. These cover the
// register/dispatch contract and every branch in command_dispatch.

static int       s_calls;
static command_t s_last;

static void spy(const command_t *cmd)
{
    s_calls++;
    s_last = *cmd;
}

static void reset(void)
{
    s_calls = 0;
    command_register_handler(NULL);  // drop any handler a prior case left set
}

static void test_dispatch_without_handler_is_noop(void)
{
    reset();
    command_t cmd = {.verb = COMMAND_DTC, .dtc_cmd = 1};
    TEST_ASSERT_FALSE(command_dispatch(&cmd));
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_dispatch_null_command_is_noop(void)
{
    reset();
    command_register_handler(spy);
    TEST_ASSERT_FALSE(command_dispatch(NULL));
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_dispatch_routes_config_to_handler(void)
{
    reset();
    command_register_handler(spy);
    vehicle_config_t c = {
        .has_speed_divisor = true, .speed_divisor = 188, .has_layout = true, .layout = 1};
    command_t cmd = {.verb = COMMAND_SET_CONFIG, .config = c};
    TEST_ASSERT_TRUE(command_dispatch(&cmd));
    TEST_ASSERT_EQUAL_INT(1, s_calls);
    TEST_ASSERT_EQUAL_INT(COMMAND_SET_CONFIG, s_last.verb);
    TEST_ASSERT_TRUE(s_last.config.has_speed_divisor);
    TEST_ASSERT_EQUAL_UINT16(188, s_last.config.speed_divisor);
    TEST_ASSERT_EQUAL_UINT8(1, s_last.config.layout);
}

static void test_dispatch_routes_dtc_raw_subcommand(void)
{
    reset();
    command_register_handler(spy);
    command_t cmd = {.verb = COMMAND_DTC, .dtc_cmd = 1};
    TEST_ASSERT_TRUE(command_dispatch(&cmd));
    TEST_ASSERT_EQUAL_INT(COMMAND_DTC, s_last.verb);
    TEST_ASSERT_EQUAL_UINT8(1, s_last.dtc_cmd);  // raw wire byte passes straight through
}

static void test_register_null_clears_handler(void)
{
    reset();
    command_register_handler(spy);
    command_t cmd = {.verb = COMMAND_DTC};
    TEST_ASSERT_TRUE(command_dispatch(&cmd));
    command_register_handler(NULL);
    TEST_ASSERT_FALSE(command_dispatch(&cmd));
    TEST_ASSERT_EQUAL_INT(1, s_calls);  // only the first dispatch reached the spy
}

void RunTests(void)
{
    RUN_TEST(test_dispatch_without_handler_is_noop);
    RUN_TEST(test_dispatch_null_command_is_noop);
    RUN_TEST(test_dispatch_routes_config_to_handler);
    RUN_TEST(test_dispatch_routes_dtc_raw_subcommand);
    RUN_TEST(test_register_null_clears_handler);
}
