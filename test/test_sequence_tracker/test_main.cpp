#include <unity.h>
#include <initializer_list>
#include "common/protocol/sequence_tracker.h"
#include "project_config.h"
using namespace adaptive_lora;
namespace {
void test_duplicate_identity() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,4})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,4})));
}
void test_all_identity_fields_participate() {
    SequenceTracker t;
    for (const PacketIdentity id : {PacketIdentity{1,2,3,4}, PacketIdentity{2,2,3,4},
                                   PacketIdentity{1,3,3,4}, PacketIdentity{1,2,4,4}, PacketIdentity{1,2,3,5}})
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe(id)));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,4})));
}
void test_out_of_order_window() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,100})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,37})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,37})));
    TEST_ASSERT_EQUAL(int(SequenceResult::STALE), int(t.observe({1,2,3,36})));
}
void test_large_jump_and_no_wrap_in_same_session() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,65535})));
    TEST_ASSERT_EQUAL(int(SequenceResult::STALE), int(t.observe({1,2,3,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,4,0})));
}
void test_sequence_exhaustion_requires_new_session() {
    SequenceCounter c; uint16_t sequence = 100;
    TEST_ASSERT_FALSE(c.take(sequence)); c.begin(123);
    for (uint32_t i = 0; i <= 65535; ++i) {
        TEST_ASSERT_TRUE(c.take(sequence)); TEST_ASSERT_EQUAL_UINT16(i, sequence);
    }
    TEST_ASSERT_FALSE(c.take(sequence)); TEST_ASSERT_FALSE(c.take(sequence));
    TEST_ASSERT_EQUAL_UINT32(123, c.bootId());
    c.begin(124); TEST_ASSERT_TRUE(c.take(sequence)); TEST_ASSERT_EQUAL_UINT16(0, sequence);
}
void test_bounded_session_replacement() {
    SequenceTracker t;
    for (uint32_t boot = 1; boot <= 9; ++boot)
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,boot,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,9,0})));
    // Evicted sessions have no retained replay state; this is a bounded M2 cache.
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,1,0})));
}
void test_wrap_safe_deadline() {
    TEST_ASSERT_FALSE(deadlineReached(0xFFFFFFF0, 0x10));
    TEST_ASSERT_TRUE(deadlineReached(0x10, 0x10));
    TEST_ASSERT_TRUE(deadlineReached(0x11, 0x10));
    TEST_ASSERT_FALSE(deadlineReached(0xFFFFFF00, 0xFFFFFF01));
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_identity); RUN_TEST(test_all_identity_fields_participate);
    RUN_TEST(test_out_of_order_window); RUN_TEST(test_large_jump_and_no_wrap_in_same_session);
    RUN_TEST(test_sequence_exhaustion_requires_new_session); RUN_TEST(test_bounded_session_replacement);
    RUN_TEST(test_wrap_safe_deadline);
    return UNITY_END();
}
