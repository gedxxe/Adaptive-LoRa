#include <unity.h>
#include <initializer_list>
#include <cstdio>
#include "app/provisioning_console.h"
using namespace adaptive_lora;
namespace {
ConsoleAction send(ProvisioningConsole& console, const char* command) {
    for (const char* p = command; *p; ++p) TEST_ASSERT_EQUAL(int(ConsoleAction::NONE), int(console.feed(*p)));
    return console.feed('\n');
}
void test_command_contract() {
    ProvisioningConsole c;
    TEST_ASSERT_EQUAL(int(ConsoleAction::HELP), int(send(c, "help")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::STATUS), int(send(c, "status")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::SHOW), int(send(c, "config show")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::SAVE), int(send(c, "config save")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::REBOOT), int(send(c, "reboot")));
}
void test_gateway_and_client_workflow() {
    ProvisioningConsole c;
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set role gateway")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set network-id 1")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set node-id 1")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set gateway-id 1")));
    TEST_ASSERT_NULL(validateConfig(c.staging()));
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set role client")));
    TEST_ASSERT_NOT_NULL(validateConfig(c.staging()));
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set node-id 2")));
    TEST_ASSERT_NULL(validateConfig(c.staging()));
}
void test_invalid_numeric_input_preserves_staging() {
    for (const char* value : {"0", "65535", "65536", "-1", "+1", "1x", "1.0", "42949672960", "999999999999999999"}) {
        ProvisioningConsole c; char command[96];
        std::snprintf(command, sizeof(command), "config set node-id %s", value);
        TEST_ASSERT_EQUAL(int(ConsoleAction::ERROR), int(send(c, command)));
        TEST_ASSERT_EQUAL_UINT16(2, c.staging().nodeId);
    }
}
void test_full_network_range_and_interval() {
    ProvisioningConsole c;
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set network-id 65535")));
    TEST_ASSERT_EQUAL_UINT16(65535, c.staging().networkId);
    TEST_ASSERT_EQUAL(int(ConsoleAction::UPDATED), int(send(c, "config set hello-interval-ms 3600000")));
    TEST_ASSERT_EQUAL(int(ConsoleAction::ERROR), int(send(c, "config set hello-interval-ms 999")));
    TEST_ASSERT_EQUAL_UINT32(3600000, c.staging().helloIntervalMs);
}
void test_unknown_extra_and_partial_commands() {
    ProvisioningConsole c;
    for (const char* command : {"status extra", "config set role", "config set role other", "config set foo 1",
                               "config set role gateway extra", "a b c d e f g", "config save now"})
        TEST_ASSERT_EQUAL(int(ConsoleAction::ERROR), int(send(c, command)));
}
void test_overflow_discards_entire_line_then_recovers() {
    ProvisioningConsole c;
    for (unsigned i = 0; i < 200; ++i) TEST_ASSERT_EQUAL(int(ConsoleAction::NONE), int(c.feed('x')));
    TEST_ASSERT_EQUAL(int(ConsoleAction::ERROR), int(c.feed('\n')));
    TEST_ASSERT_EQUAL_STRING("line_too_long", c.error());
    TEST_ASSERT_EQUAL(int(ConsoleAction::STATUS), int(send(c, "status")));
}
void test_crlf_empty_and_spacing() {
    ProvisioningConsole c;
    TEST_ASSERT_EQUAL(int(ConsoleAction::NONE), int(c.feed('\n')));
    for (const char* p = "status"; *p; ++p) c.feed(*p);
    TEST_ASSERT_EQUAL(int(ConsoleAction::STATUS), int(c.feed('\r')));
    TEST_ASSERT_EQUAL(int(ConsoleAction::NONE), int(c.feed('\n')));
    TEST_ASSERT_EQUAL(int(ConsoleAction::SHOW), int(send(c, "  config   show  ")));
}
void test_staging_does_not_change_loaded_config() {
    DeviceConfig loaded; loaded.role = DeviceRole::CLIENT;
    ProvisioningConsole c; c.begin(loaded);
    send(c, "config set node-id 3"); send(c, "config set role gateway");
    TEST_ASSERT_EQUAL_UINT16(2, loaded.nodeId);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(DeviceRole::CLIENT), uint8_t(loaded.role));
    TEST_ASSERT_EQUAL_UINT16(3, c.staging().nodeId);
}
void test_invalid_config_cannot_encode_for_save() {
    ProvisioningConsole c; send(c, "config set role gateway");
    uint8_t blob[kConfigBlobSize];
    TEST_ASSERT_FALSE(encodeConfig(c.staging(), blob, sizeof(blob)));
    send(c, "config set node-id 1");
    TEST_ASSERT_TRUE(encodeConfig(c.staging(), blob, sizeof(blob)));
}
void test_binary_input_rejected() {
    ProvisioningConsole c;
    c.feed('s'); c.feed('\0');
    TEST_ASSERT_EQUAL(int(ConsoleAction::ERROR), int(c.feed('\n')));
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_command_contract); RUN_TEST(test_gateway_and_client_workflow);
    RUN_TEST(test_invalid_numeric_input_preserves_staging); RUN_TEST(test_full_network_range_and_interval);
    RUN_TEST(test_unknown_extra_and_partial_commands); RUN_TEST(test_overflow_discards_entire_line_then_recovers);
    RUN_TEST(test_crlf_empty_and_spacing); RUN_TEST(test_staging_does_not_change_loaded_config);
    RUN_TEST(test_invalid_config_cannot_encode_for_save); RUN_TEST(test_binary_input_rejected);
    return UNITY_END();
}
